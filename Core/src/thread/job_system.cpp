//
// Created by unite on 22.05.2024.
//

#include <thread/job_system.h>

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
/*
Job::Job(const Job& job)
{
	hasStarted_ = job.hasStarted_.load(std::memory_order_acquire);
	isDone_ = job.isDone_.load(std::memory_order_acquire);
}


Job::Job(Job&& job) noexcept
{
	hasStarted_ = job.hasStarted_.load(std::memory_order_acquire);
	isDone_ = job.isDone_.load(std::memory_order_acquire);
}

Job& Job::operator=(const Job& job)
{
	hasStarted_ = job.hasStarted_.load(std::memory_order_acquire);
	isDone_ = job.isDone_.load(std::memory_order_acquire);
	return *this;
}

Job& Job::operator=(Job&& job) noexcept
{
	hasStarted_ = job.hasStarted_.load(std::memory_order_acquire);
	isDone_ = job.isDone_.load(std::memory_order_acquire);
	return *this;
}
*/

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

void WorkerQueue::Begin()
{
    isRunning_.store(true, std::memory_order_release);
}

void WorkerQueue::AddJob(Job* newJob)
{
    std::scoped_lock lock(mutex_);
    jobsQueue_.push(newJob);
    conditionVariable_.notify_one();
}

bool WorkerQueue::IsEmpty() const
{
    std::shared_lock lock(mutex_);
    return jobsQueue_.empty();
}

bool WorkerQueue::IsRunning() const
{
    return isRunning_.load(std::memory_order_acquire);
}

Job* WorkerQueue::PopNextTask()
{
    if (IsEmpty())
        return nullptr;

    std::scoped_lock lock(mutex_);
    if (jobsQueue_.empty())
        return nullptr;
    auto newTask = jobsQueue_.front();
    jobsQueue_.pop();
    return newTask;
}

void WorkerQueue::WaitForTask()
{
    std::unique_lock lock(mutex_);
    if(!isRunning_.load(std::memory_order_acquire))
        return;
	if(jobsQueue_.empty())
	{
		conditionVariable_.wait(lock);
	}
}

void WorkerQueue::End()
{
    isRunning_.store(false, std::memory_order_release);
    conditionVariable_.notify_all();
}

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

void Worker::Run()
{
    if(queue_ == nullptr)
        return;
    while(queue_->IsRunning())
    {
        if (queue_->IsEmpty())
        {
            if (!queue_->IsRunning())
            {
                return;
            }
            queue_->WaitForTask();
        }
        else
        {
            while (!queue_->IsEmpty())
            {
                auto newTask = queue_->PopNextTask();
                if (newTask == nullptr)
                    continue;
                if (!newTask->ShouldStart())
                {
                    queue_->AddJob(newTask);
                }
                else
                {
                    newTask->Execute();
                }
            }
        }
    }
    // Even when not running anymore we still need to finish the remaining jobs
    while (!queue_->IsEmpty())
    {
        auto newTask = queue_->PopNextTask();
        if (newTask == nullptr)
            continue;
        if (!newTask->ShouldStart())
        {
            queue_->AddJob(newTask);
        }
        else
        {
            newTask->Execute();
        }
    }
}

static JobSystem* instance = nullptr;

JobSystem::JobSystem()
{
    instance = this;
	queues_.reserve(10);
	workers_.reserve(std::thread::hardware_concurrency());
}

int JobSystem::SetupNewQueue(int threadCount)
{
    const int newQueueIndex = static_cast<int>(queues_.size());
    queues_.emplace_back();
    for(int i = 0; i < threadCount; i++)
    {
        workers_.emplace_back(&queues_.back());
    }
    return newQueueIndex;
}

void JobSystem::Begin()
{
    for(auto& queue: queues_)
    {
        queue.Begin();
    }
    for(auto& worker : workers_)
    {
        worker.Begin();
    }
}

void JobSystem::AddJob(Job* newJob, int queueIndex)
{
    newJob->Reset();
    if(queueIndex == MAIN_QUEUE_INDEX)
    {
        mainThreadQueue_.AddJob(newJob);
        return;
    }
    queues_[queueIndex].AddJob(newJob);
}

void JobSystem::End()
{
    for(auto& queue: queues_)
    {
        queue.End();
    }
    for(auto& worker: workers_)
    {
        worker.End();
    }
}

void JobSystem::ExecuteMainThread()
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

JobSystem::~JobSystem()
{
    instance = nullptr;
}

JobSystem* GetJobSystem()
{
    if(instance == nullptr)
    {
        // Instance should not be nullptr
        // It means the JobSystem was not instanced somewhere
        // Or that it was destroyed (did you check the scope)
        std::terminate();
    }
    return instance;
}
}