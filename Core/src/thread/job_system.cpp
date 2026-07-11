#include "thread/job_system.h"
#if defined(__clang__)
#pragma clang diagnostic push
#pragma clang diagnostic ignored "-Wold-style-cast"
#elif defined(__GNUC__)
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wold-style-cast"
#endif
#if defined(NN_NINTENDO_SDK) && !defined(__unix__)
#define NEKO_DEFINED_UNIX_FOR_CONCURRENTQUEUE
#define __unix__
#endif
#include <blockingconcurrentqueue.h>
#ifdef NEKO_DEFINED_UNIX_FOR_CONCURRENTQUEUE
#undef __unix__
#undef NEKO_DEFINED_UNIX_FOR_CONCURRENTQUEUE
#endif
#if defined(__clang__)
#pragma clang diagnostic pop
#elif defined(__GNUC__)
#pragma GCC diagnostic pop
#endif
#include <thread>


#ifdef TRACY_ENABLE
#include <tracy/Tracy.hpp>
#endif
#include <algorithm>
#include <ranges>

namespace neko
{

void Job::Execute()
{
#ifdef TRACY_ENABLE
    ZoneScoped;
#endif
    hasStarted_.store(true, std::memory_order_release);
    if (IsCancelled())
    {
        failed_.store(true, std::memory_order_release);
        isDone_.store(true, std::memory_order_release);
        isDone_.notify_all();
        return;
    }
    try
    {
        ExecuteImpl();
    }
    catch (...)
    {
        failed_.store(true, std::memory_order_release);
    }
    isDone_.store(true, std::memory_order_release);
    isDone_.notify_all();
}

bool Job::HasStarted() const
{
    return hasStarted_.load(std::memory_order_acquire);
}

bool Job::IsDone() const
{
    return isDone_.load(std::memory_order_acquire);
}

bool Job::ShouldStart() const
{
    return true;
}

void Job::Reset()
{
    hasStarted_.store(false, std::memory_order_release);
    isDone_.store(false, std::memory_order_release);
    failed_.store(false, std::memory_order_release);
}

bool Job::CheckDependency([[maybe_unused]]const Job *ptr) const
{
    return false;
}

bool Job::HasFailed() const
{
    return failed_.load(std::memory_order_acquire);
}

bool Job::IsCancelled() const
{
    return cancelFlag_ != nullptr && cancelFlag_->load(std::memory_order_acquire);
}

void Job::SkipAsFailed()
{
    hasStarted_.store(true, std::memory_order_release);
    failed_.store(true, std::memory_order_release);
    isDone_.store(true, std::memory_order_release);
    isDone_.notify_all();
}

void Job::MarkStarted()
{
    hasStarted_.store(true, std::memory_order_release);
}

void Job::MarkDone()
{
    isDone_.store(true, std::memory_order_release);
    isDone_.notify_all();
}

void Job::MarkFailed()
{
    failed_.store(true, std::memory_order_release);
}

void Job::Join() const
{

#ifdef TRACY_ENABLE
    ZoneScoped;
#endif
    while(!IsDone())
    {
        isDone_.wait(false, std::memory_order_acquire);
    }
}


bool DependentJob::ShouldStart() const
{
    if(dependency_ != nullptr)
    {
        return dependency_->IsDone();
    }
    return false;
}

bool DependentJob::CheckDependency(const Job *ptr) const
{
#ifdef TRACY_ENABLE
    ZoneScoped;
#endif
    if(ptr == this)
    {
        return true;
    }
    auto dep = dependency_;
    if(dep != nullptr) {
        return dep->CheckDependency(ptr);
    }
    return false;
}

void DependentJob::Execute()
{
#ifdef TRACY_ENABLE
    ZoneScoped;
#endif
    if(dependency_ != nullptr)
    {
		dependency_->Join();
        if (dependency_->HasFailed())
        {
            SkipAsFailed();
            return;
        }
    }
    Job::Execute();
}

bool DependenciesJob::ShouldStart() const
{
#ifdef TRACY_ENABLE
    ZoneScoped;
#endif
    bool shouldStart = true;
    for (auto& dependency : dependencies_)
    {
        if (dependency != nullptr && !dependency->IsDone())
        {
            shouldStart = false;
            break;
        }
    }
    return shouldStart;
}

bool DependenciesJob::AddDependency(Job* dependency)
{
    if(dependency == nullptr || dependency->CheckDependency(this))
    {
        return false;
    }
    dependencies_.push_back(dependency);
    return true;
}

bool DependenciesJob::CheckDependency(const Job *ptr) const
{
    if(ptr == this)
    {
        return true;
    }
	return std::ranges::any_of(dependencies_, [ptr](const auto* dep){
		return dep->CheckDependency(ptr);
	});
}

void DependenciesJob::Execute()
{

#ifdef TRACY_ENABLE
    ZoneScoped;
#endif
    for(auto& dependency : dependencies_)
    {
        if(dependency != nullptr)
        {
			dependency->Join();
            if (dependency->HasFailed())
            {
                SkipAsFailed();
                return;
            }
        }
    }
    Job::Execute();
}


bool ScheduleJob::ShouldStart() const
{
    return dependency_ == nullptr || dependency_->HasStarted();
}

bool ScheduleJob::CheckDependency(const Job* ptr) const
{
#ifdef TRACY_ENABLE
    ZoneScoped;
#endif
    if (ptr == this)
    {
        return true;
    }
    return dependency_ != nullptr && dependency_->CheckDependency(ptr);
}

void ScheduleJob::Execute()
{

#ifdef TRACY_ENABLE
    ZoneScoped;
#endif
    MarkStarted();

    if (dependency_ != nullptr)
    {
        dependency_->Join();
    }

    // Always schedule the contained job, even on upstream failure / cancellation,
    // so downstream consumers joining on it never deadlock.  The contained job's
    // own cancel flag handles cancellation end-to-end.
    if (containedJob_ != nullptr)
    {
        JobSystem::AddJob(containedJob_, queueIndex_);
    }

    if (IsCancelled() || (dependency_ != nullptr && dependency_->HasFailed()))
    {
        MarkFailed();
    }
    MarkDone();
}


class WorkerQueue
{
public:
    WorkerQueue() = default;
    WorkerQueue(const WorkerQueue&) = delete;
    WorkerQueue& operator= (const WorkerQueue&) = delete;
    WorkerQueue(WorkerQueue&&) noexcept{}
    WorkerQueue& operator= (WorkerQueue&&) noexcept{ return *this; }

    void AddJob(Job* newJob);
    bool IsEmpty() const;
    Job* PopNextTask();
    bool WaitDequeue(Job*& out, std::int64_t timeoutUsecs);
    void End();
private:
    moodycamel::BlockingConcurrentQueue<Job*> jobsQueue_;
};




class Worker
{
public:
    Worker(std::size_t queueIndex) : queueIndex_(queueIndex){}
    void Begin();
    void End();
private:
    void Run() const;
    std::thread thread_;
    std::size_t queueIndex_ = std::numeric_limits<size_t>::max();
};

void Worker::Begin()
{
    thread_ = std::thread(&Worker::Run, this);
}

void Worker::End()
{
    if(thread_.joinable())
    {
        thread_.join();
    }
}



namespace JobSystem
{
namespace
{
WorkerQueue mainThreadQueue_{};
std::vector<WorkerQueue> queues_{};
std::vector<Worker> workers_{};
std::atomic<bool> isRunning_{ false };
}

int SetupNewQueue(int threadCount)
{
    const int newQueueIndex = static_cast<int>(queues_.size());
    queues_.emplace_back();
    for(int i = 0; i < threadCount; i++)
    {
        workers_.emplace_back(newQueueIndex);
    }
    return newQueueIndex;
}

void Begin()
{
    isRunning_.store(true, std::memory_order_release);
    for(auto& worker : workers_)
    {
        worker.Begin();
    }
}

void AddJob(Job* newJob, int queueIndex)
{
#ifdef TRACY_ENABLE
    ZoneScoped;
#endif
    newJob->Reset();
    if(queueIndex == MAIN_QUEUE_INDEX)
    {
        mainThreadQueue_.AddJob(newJob);
        return;
    }
    queues_[queueIndex].AddJob(newJob);
}

void End()
{

    isRunning_.store(false, std::memory_order_release);
    for(auto& queue: queues_)
    {
        queue.End();
    }
    for(auto& worker: workers_)
    {
        worker.End();
    }
    queues_.clear();
    workers_.clear();
}

void ExecuteMainThread()
{
    while (auto newTask = mainThreadQueue_.PopNextTask())
    {
        if (!newTask->ShouldStart())
        {
            mainThreadQueue_.AddJob(newTask);
            std::this_thread::yield();
        }
        else
        {
            newTask->Execute();
        }
    }
}


}
void Worker::Run() const
{
    auto& queue = JobSystem::queues_[queueIndex_];
    constexpr std::int64_t waitTimeoutUsecs = 250;
    while(JobSystem::isRunning_.load(std::memory_order_acquire))
    {
        Job* newTask = nullptr;
        if (!queue.WaitDequeue(newTask, waitTimeoutUsecs) || newTask == nullptr)
        {
            continue;
        }

        if (!newTask->ShouldStart())
        {
            queue.AddJob(newTask);
            std::this_thread::yield();
            continue;
        }
        newTask->Execute();
    }
    // Even when not running anymore we still need to finish the remaining jobs
    while (!queue.IsEmpty())
    {
        auto newTask = queue.PopNextTask();
        if (newTask == nullptr)
            continue;
        if (!newTask->ShouldStart())
        {
            queue.AddJob(newTask);
            std::this_thread::yield();
        }
        else
        {
            newTask->Execute();
        }
    }
}


void WorkerQueue::AddJob(Job* newJob)
{
    jobsQueue_.enqueue(newJob);
}

bool WorkerQueue::IsEmpty() const
{
    // size_approx() is only an approximate empty hint; dequeue operations are the
    // correctness gate.
    return jobsQueue_.size_approx() == 0;
}


Job* WorkerQueue::PopNextTask()
{
    Job* newTask = nullptr;
    if (!jobsQueue_.try_dequeue(newTask))
    {
        return nullptr;
    }
    return newTask;
}

bool WorkerQueue::WaitDequeue(Job*& out, std::int64_t timeoutUsecs)
{
    return jobsQueue_.wait_dequeue_timed(out, timeoutUsecs);
}

void WorkerQueue::End()
{
}
}
