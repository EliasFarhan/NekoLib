#include "thread/job_system.h"
#include <concurrentqueue.h>

namespace neko
{



void Job::Execute()
{
    hasStarted_.store(true, std::memory_order_release);
    ExecuteImpl();
    isDone_.store(true, std::memory_order_release);
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
}

bool Job::CheckDependency([[maybe_unused]]const Job *ptr) const
{
    return false;
}

void Job::Join()
{
    while(!IsDone())
    {
    }
}

void FuncJob::ExecuteImpl()
{
    func_();
}

bool FuncDependentJob::ShouldStart() const
{
    if(dependency_ != nullptr)
    {
        return dependency_->HasStarted();
    }
    return false;
}

bool FuncDependentJob::CheckDependency(const Job *ptr) const
{
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

void FuncDependentJob::Execute()
{
    if(dependency_ != nullptr)
    {
		dependency_->Join();
    }
    Job::Execute();
}

bool FuncDependenciesJob::ShouldStart() const
{
    bool shouldStart = true;
    for (auto& dependency : dependencies_)
    {
        if (dependency != nullptr && !dependency->HasStarted())
        {
            shouldStart = false;
            break;
        }
    }
    return shouldStart;
}

bool FuncDependenciesJob::AddDependency(Job* dependency)
{
    if(dependency == nullptr || dependency->CheckDependency(this))
    {
        return false;
    }
    dependencies_.push_back(dependency);
    return true;
}

bool FuncDependenciesJob::CheckDependency(const Job *ptr) const
{
    if(ptr == this)
    {
        return true;
    }
	return std::any_of(dependencies_.begin(), dependencies_.end(), [ptr](const auto* dep){
		return dep->CheckDependency(ptr);
	});
}

void FuncDependenciesJob::Execute()
{
    for(auto& dependency : dependencies_)
    {
        if(dependency != nullptr)
        {
			dependency->Join();
        }
    }
    Job::Execute();
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
    void End();
private:
    moodycamel::ConcurrentQueue<Job*> jobsQueue_;
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
    while (!mainThreadQueue_.IsEmpty())
    {
        auto newTask = mainThreadQueue_.PopNextTask();
        if (!newTask->ShouldStart())
        {
            mainThreadQueue_.AddJob(newTask);
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
    while(JobSystem::isRunning_.load(std::memory_order_acquire))
    {
        if (queue.IsEmpty())
        {
            if (!JobSystem::isRunning_.load(std::memory_order_acquire))
            {
                return;
            }
        }
        else
        {
            while (!queue.IsEmpty())
            {
                auto newTask = queue.PopNextTask();
                if (newTask == nullptr)
                    continue;
                if (!newTask->ShouldStart())
                {
                    queue.AddJob(newTask);
                }
                else
                {
                    newTask->Execute();
                }
            }
        }
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
    return jobsQueue_.size_approx() == 0;
}


Job* WorkerQueue::PopNextTask()
{
    if (IsEmpty())
        return nullptr;

    Job* newTask = nullptr;
    jobsQueue_.try_dequeue(newTask);
    return newTask;
}

void WorkerQueue::End()
{
    while (!IsEmpty()) {

    }
}
}