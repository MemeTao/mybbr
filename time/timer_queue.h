#ifndef COMMON_TIMER_QUEUE_H_
#define COMMON_TIMER_QUEUE_H_
#include <set>
#include <thread>
#include <mutex>
#include <condition_variable>
//#include <future>
#include <functional>
#include <memory>
#include <atomic>
#include <vector>
#include <rtc/common/time_stamp.h>

namespace rtc
{
namespace common
{

class Timer
{
public:
    using TimerCallback  = std::function<void(void)>;
public:
    Timer(const Timestamp& when, /*unix timestamp*/
          const uint64_t interval/*us*/,
          const TimerCallback& cb);
    void run(void);
    Timestamp expiration()const
    {
        return expiration_;
    }
    bool is_repeat(void)const
    {
        return repeat_;
    }
    void restart(Timestamp now);
    int64_t sequence()const
    {
        return sequence_;
    }
private:
    Timestamp expiration_;
    bool repeat_;
    uint64_t interval_;
    TimerCallback callback_;
    const int64_t sequence_;
    static std::atomic<int64_t> seq_creator_;
};
class TimerId
{
public:
    TimerId()
        :timer_(nullptr),
        sequence_(0)
    {}
    TimerId(const std::shared_ptr<Timer> timer, const int64_t seq)
        :timer_(timer),
        sequence_(seq)
    {}
    //default copy-ctor, dtor and assignment are okay
    friend class TimerQueue;
private:
    std::shared_ptr<Timer> timer_;
    int64_t sequence_;
};
/*run_after,
 * run_when,
 * run_every,
 * cacel are enough
 */
class TimerQueue
{
    class Thread
    {
        using TaskInterface = std::function<void(void)>;
    public:
        Thread(TimerQueue* p_timer_queue);
        ~Thread();
        void loop();
        //thread safe
        inline void assert_in_loop_thread();
        void run_in_loop(TaskInterface&& task);
        bool is_in_loop_thread()
        {
            return std::this_thread::get_id() == thread_id_;
        }
        inline void wake_up();
        //following functions are called in loop thread
        //needn't to lock
        void reset_wait_for_time(const Timestamp& when);
        void exit()
        {
            running_ = false;
        }
    private:
        common::TimeDelta get_wait_time();
        void wait_for(common::TimeDelta dt);
        void proc();
        void do_tasks();
        TimerQueue* ptr_queue_;
        volatile bool running_;
        std::thread thread_loop_;
        std::thread::id thread_id_;
        Timestamp wait_for_when_;
        std::mutex mutex_wait_for_times_; //only use for sleep
        std::condition_variable condition_;
        bool signaled_ = false;
        std::vector<TaskInterface> tasks_ready_do_;
        //common::SpinLock mutex_tasks_;
        std::mutex mutex_tasks_;
    };
    //for find
    using Entry = std::pair<Timestamp,std::shared_ptr<Timer>>;
    using TimerList = std::set<Entry>;
    //for delete
    using ActiveTimer =  std::pair<std::shared_ptr<Timer>, int64_t>;
    using ActiveTimerSet =  std::set<ActiveTimer>;
public:
    TimerQueue();
    ~TimerQueue() = default;
    bool in_timer_queue_thread()
    {
        return loop_.is_in_loop_thread();
    }
    //XXX:run() is not thread safe,please be careful
    void run(); //启动定时服务
    //following functions are thread safe
    void exit(); //关闭定时服务
    TimerId run_after(const TimeDelta delay,Timer::TimerCallback&& cb);
    //legacy:not safe when user modify system time
    //TimerId run_when(const Timestamp when,Timer::TimerCallback&& cb);
    TimerId run_every(const TimeDelta interval,Timer::TimerCallback&& cb);
    void cancel(const TimerId& id);

private:
    bool is_exsits_expired();
    void handle_expired();
    void add_timer(std::shared_ptr<Timer> timer);
    bool insert(std::shared_ptr<Timer> timer);
    void erase(const TimerId& id);
    std::vector<TimerQueue::Entry> get_expired(Timestamp now);
    void reset(const std::vector<Entry>& expired, Timestamp now);
    TimerList timers_;
    ActiveTimerSet active_timers_;
    ActiveTimerSet cancelingTimers_;
    Thread loop_;
    std::atomic<bool> calling_expired_timer_;
};

}  //namespace common
}
#endif
