#include <rtc/common/timer_queue.h>
#include <cassert>
#include <rtc/common/alog/logger.h>

namespace rtc
{
namespace common
{ 
using namespace time_interval;

std::atomic<int64_t> Timer::seq_creator_{1};
Timer::Timer(const Timestamp& when,uint64_t interval,const TimerCallback& cb)
    :expiration_(when),
    repeat_( interval > 0),
    interval_(interval),
    callback_(std::move(cb)),
    sequence_(seq_creator_.fetch_add(1))
{
    ;
}
void Timer::run(void)
{
    if(callback_)
    {
        callback_();
    }
}

void Timer::restart(Timestamp now)
{
    expiration_ = Timestamp(now.microseconds() + static_cast<int64_t>(interval_));
}

TimerQueue::Thread::Thread(TimerQueue* p)
    :ptr_queue_(p),
    running_(false),
    wait_for_when_(0)
{
}
TimerQueue::Thread::~Thread()
{
    //assert(running_ == false);
    run_in_loop([this](){
        running_ = false;
    });
    if(thread_loop_.joinable()){
        thread_loop_.join();
    }
}
void TimerQueue::Thread::loop()
{
    if(running_)
    {
        return;
    }
    thread_loop_ = std::thread(std::bind(&TimerQueue::Thread::proc,this));
    while(!running_ ){}; //等待runing
}

inline void TimerQueue::Thread::assert_in_loop_thread()
{
    assert(running_ && std::this_thread::get_id() == thread_id_);
    //FIXME:print log when on release mode
}

inline void TimerQueue::Thread::wake_up()
{
    std::unique_lock<std::mutex> lock(mutex_wait_for_times_,std::defer_lock_t());
    lock.lock();
    signaled_ = true;
    lock.unlock();
    condition_.notify_one();
}

void TimerQueue::Thread::wait_for(common::TimeDelta dt)
{
    const int64_t wait_us = dt.value();
    if(wait_us <= 0)
    {
        return;
    }
    std::unique_lock<std::mutex> lock(mutex_wait_for_times_,std::defer_lock_t());
    //XXX:如果需要等待100us以上的时间，就休眠。否则的话，就不停的轮询(占用该cpu 100us时间)
    //std::unique_lock<std::mutex> lock(mutex_wait_for_times_);
    if( wait_us >= 100)
    {  
        lock.lock();
        while(!signaled_){
            auto status = condition_.wait_for(lock, std::chrono::microseconds(wait_us));
            if(status == std::cv_status::timeout) {
                break;
            }else {
                break;  //windows下面条件变量在没有notify、没有超时的情况下也他么的能醒来，我佛了(vs2017)
            }
        }
        signaled_  = false;
        lock.unlock();
        //condition_.wait_for(lock, std::chrono::microseconds(wait_us));
    }
    else
    {
        //sleep(0);
    }
}

common::TimeDelta TimerQueue::Thread::get_wait_time()
{
    auto dt = 0_ms;
    {
        std::lock_guard<std::mutex> lock_tasks(mutex_tasks_);
        //common::SpinLockGuard lock_tasks(mutex_tasks_);
        if(tasks_ready_do_.size())
        {
            return dt; 
        }
    }
    if(ptr_queue_->timers_.size() == 0)
    {
        dt =  100_ms;  //如果定时器队列为空，任务队列为空
    }
    else
    {
        dt = wait_for_when_ - Timestamp::now(Timestamp::kSincePowerup);
    }
    return dt;
}

void TimerQueue::Thread::reset_wait_for_time(const Timestamp& when)
{
    wait_for_when_ = when;
}

void TimerQueue::Thread::proc()
{
    assert(ptr_queue_ != nullptr);
    thread_id_ = std::this_thread::get_id();
    running_ = true;

    while(running_)
    {
        //how much time need to wait
        wait_for(get_wait_time());
        //check is there timer expired,find it and do callback
        if(ptr_queue_->is_exsits_expired())
        {
            ptr_queue_->handle_expired();
        }
        do_tasks();
    }
}

void TimerQueue::Thread::do_tasks()
{
    std::vector<TaskInterface> tasks;
    {
        std::lock_guard<std::mutex> mutex(mutex_tasks_);
        //common::SpinLockGuard lock_tasks(mutex_tasks_);
        if(tasks_ready_do_.size() > 0)
        {
            tasks.swap(tasks_ready_do_);
        }
    }
    for(const auto& task:tasks)
    {
        task();
    }
}

void TimerQueue::Thread::run_in_loop(TaskInterface&& task)
{
    if(std::this_thread::get_id() == thread_id_)
    {
        task();
    }
    else
    {
        {
            std::lock_guard<std::mutex> mutex(mutex_tasks_);
            //common::SpinLockGuard lock_tasks(mutex_tasks_);
            tasks_ready_do_.push_back(std::move(task));
        }
        wake_up();
    }
}
TimerQueue::TimerQueue()
    :loop_(this),
     calling_expired_timer_(false)
{}

void TimerQueue::run()
{
    loop_.loop();
}

void TimerQueue::exit()
{
    loop_.run_in_loop(std::bind(&TimerQueue::Thread::exit,&loop_));
}

void TimerQueue::handle_expired()
{
    loop_.assert_in_loop_thread();
    auto now = common::Timestamp::now(Timestamp::kSincePowerup);
    std::vector<Entry> expired = get_expired(now);
    calling_expired_timer_ = true;
    cancelingTimers_.clear();
    // safe to callback outside critical section
    for (std::vector<Entry>::iterator it = expired.begin();
        it != expired.end(); ++it)
    {
        it->second->run();
    }
    calling_expired_timer_ = false;
    reset(expired, now);
}

TimerId TimerQueue::run_after(const TimeDelta delay,Timer::TimerCallback&& cb)
{
    Timestamp when = Timestamp::now(Timestamp::kSincePowerup) + delay;
    std::shared_ptr<Timer> t (new Timer(when,0,std::move(cb)));
    loop_.run_in_loop(std::bind(&TimerQueue::add_timer,this,t));
    return TimerId(t,t->sequence());
}

//TimerId TimerQueue::run_when(const Timestamp when,Timer::TimerCallback&& cb)
//{
//    std::shared_ptr<Timer> t(new Timer(when,0,std::move(cb)));
//    loop_.run_in_loop(std::bind(&TimerQueue::add_timer,this,t));
//    return TimerId(t,t->sequence());
//}

TimerId TimerQueue::run_every(const TimeDelta interval,Timer::TimerCallback&& cb)
{
    Timestamp when = Timestamp::now(Timestamp::kSincePowerup) + interval;
    uint64_t interval_us = static_cast<uint64_t>(interval.value());
    std::shared_ptr<Timer> t(new Timer
                (when,
                interval_us,
                std::move(cb)));
    loop_.run_in_loop(std::bind(&TimerQueue::add_timer,this,t));
    return TimerId(t,t->sequence());
}
//only call in loop thread
void TimerQueue::add_timer(std::shared_ptr<Timer> timer)
{
    loop_.assert_in_loop_thread();
    bool earliest_changed = insert(timer);
    if(earliest_changed)
    {
        loop_.reset_wait_for_time(timer->expiration()); 
    }
}
bool TimerQueue::insert(std::shared_ptr<Timer> timer)
{
    loop_.assert_in_loop_thread();
    assert(timers_.size() == active_timers_.size());
    bool earliest_changed = false;
    Timestamp when = timer->expiration();
    TimerList::iterator it = timers_.begin();
    if (it == timers_.end() || when < it->first)
    {
        earliest_changed = true;
    }
    //add timer in times_
    {
        std::pair<TimerList::iterator, bool> result
            = timers_.insert(Entry(when, timer));
        assert(result.second); 
        (void)result;
    }
    //add timer in active_timers_
    {
        std::pair<ActiveTimerSet::iterator, bool> result
            = active_timers_.insert(ActiveTimer(timer, timer->sequence()));
        assert(result.second); 
        (void)result;
    }
    assert(timers_.size() == active_timers_.size());
    return earliest_changed;
}

std::vector<TimerQueue::Entry> TimerQueue::get_expired(Timestamp now)
{
    loop_.assert_in_loop_thread();
    assert(timers_.size() == active_timers_.size());
    std::vector<Entry> expired;
    //Entry sentry(now, reinterpret_cast<Timer*>(UINTPTR_MAX));
    Entry sentry(now,(nullptr));
    TimerList::iterator end = timers_.lower_bound(sentry);
    assert(end == timers_.end() || now <= end->first);
    std::copy(timers_.begin(), end, back_inserter(expired));
    timers_.erase(timers_.begin(), end);
    for (const Entry& it : expired)
    {
        ActiveTimer timer(it.second, it.second->sequence());
        size_t n = active_timers_.erase(timer);
        assert(n == 1);
        (void) n;
    }
    assert(timers_.size() == active_timers_.size());
    return expired;
}
void TimerQueue::reset(const std::vector<Entry>& expired, Timestamp now)
{
    loop_.assert_in_loop_thread();
    Timestamp next_expire;

    for (std::vector<Entry>::const_iterator it = expired.begin();
            it != expired.end(); ++it)
    {
        ActiveTimer timer(it->second, it->second->sequence());
        if (it->second->is_repeat()
            && cancelingTimers_.find(timer) == cancelingTimers_.end())
        {
            it->second->restart(now);
            insert(it->second);
        }
    }
    if (!timers_.empty())
    {
        next_expire = timers_.begin()->second->expiration();
        loop_.reset_wait_for_time(next_expire);
    }
}
bool TimerQueue::is_exsits_expired()
{
    loop_.assert_in_loop_thread();
    bool ret = false;
    if (!timers_.empty())
    {
        auto next_expire = timers_.begin()->second->expiration();
        if(next_expire < Timestamp::now(Timestamp::kSincePowerup))
        {
            ret = true;
        }
    }
    return ret;
}

void TimerQueue::cancel(const TimerId& id)
{
    loop_.run_in_loop(std::bind(&TimerQueue::erase,this,id));
}

void TimerQueue::erase(const TimerId& id)
{
    loop_.assert_in_loop_thread();
    assert(timers_.size() == active_timers_.size());
    ActiveTimer item(id.timer_,id.sequence_);
    ActiveTimerSet::iterator it = active_timers_.find(item);
    if(it != active_timers_.end())
    {
        //timers delete it first
        size_t n = timers_.erase(Entry(it->first->expiration(), it->first));
        assert(n == 1);(void) n ;
        active_timers_.erase(item);
    }
    else if(calling_expired_timer_)
    {   //定时器回调中调用 cancle(id)
        cancelingTimers_.insert(item);
    }
    assert(active_timers_.size() == timers_.size());
}

} //namespace common
}
