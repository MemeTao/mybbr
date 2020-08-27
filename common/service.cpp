#include <rtc/common/service.h>
#include <rtc/common/timer_queue.h>

namespace rtc{
namespace common{
std::mutex Services::m;

std::shared_ptr<TimerQueue> Services::timer_queue(Services::Type type)
{
    static std::shared_ptr<TimerQueue> global_queue_ = std::make_shared<TimerQueue>();
    static std::shared_ptr<TimerQueue> pacer_queue_ = std::make_shared<TimerQueue>();
    static std::shared_ptr<TimerQueue> gcc_queue_ = std::make_shared<TimerQueue>();
    static bool global_inilialized = false;
    static bool pacer_inilialized = false;
    static bool gcc_inilialized = false;
    std::shared_ptr<TimerQueue> queue = nullptr;
    bool* inilialized = nullptr;
    switch(type)
    {
    case Type::kGlobalTimerQueue:
        inilialized = &global_inilialized;
        queue = global_queue_;
        break;
    case Type::kPacerTimerQueue:
        inilialized = &pacer_inilialized;
        queue = pacer_queue_;
        break;
    case Type::kGccTimerQueue:
        inilialized = &gcc_inilialized;
        queue = gcc_queue_;
        break;
    default:
        break;
    }
    if(!(*inilialized))
    {
        std::lock_guard<std::mutex> lock(Services::m);
        if(!(*inilialized))
        {
            queue->run();
            *inilialized = true;
        }
    }
    return queue;
}


}
}
