#ifndef NEKOLIB_JOB_SYSTEM_H
#define NEKOLIB_JOB_SYSTEM_H

#include <vector>
#include <atomic>
#include <functional>
#include <memory>
#include <shared_mutex>
#include <queue>
#include <condition_variable>
#include <thread>
#include <future>

namespace neko
{

class Job
{
public:
    Job();
    virtual ~Job() = default;
    virtual void Execute();
    bool HasStarted() const;
    bool IsDone() const;
    virtual bool ShouldStart() const;
    void Reset();
    void Join();

    /**
     * \brief CheckDependency is a member function used to check if the arg ptr is already a dependency
     * @param ptr
     * @return false if not a dependency
     */
    virtual bool CheckDependency(const Job *ptr) const;

protected:
    virtual void ExecuteImpl() = 0;

private:
    std::promise<void> promise_;
    std::shared_future<void> taskDoneFuture_;
    std::atomic<bool> hasStarted_{ false };
    std::atomic<bool> isDone_{ false };
};

class FuncJob : public Job
{
public:
	FuncJob() = default;
    explicit FuncJob(const std::function<void(void)>& func): Job(), func_(func){}
protected:
    void ExecuteImpl() override;
private:
    std::function<void(void)> func_{};
};

class FuncDependentJob : public FuncJob
{
public:
    FuncDependentJob(Job* dependency, const std::function<void(void)>& func) :
            FuncJob(func),
            dependency_(dependency)
    {

    }
    void Execute() override;
    [[nodiscard]] bool ShouldStart() const override;
	[[nodiscard]] bool CheckDependency(const Job *ptr) const override;
private:
    Job* dependency_{};
};

class FuncDependenciesJob: public FuncJob
{
public:
    explicit FuncDependenciesJob(const std::function<void(void)>& func): FuncJob(func)
    {}
    FuncDependenciesJob(std::initializer_list<Job*> dependencies, const std::function<void(void)>& func):
            FuncJob(func), dependencies_(dependencies)
    {}
    [[nodiscard]] bool ShouldStart() const override;

    bool AddDependency(Job* dependency);
    void Execute() override;
protected:
    bool CheckDependency(const Job *ptr) const override;
    std::vector<Job*> dependencies_{};
};

class WorkerQueue
{
public:
    WorkerQueue() = default;
    WorkerQueue(const WorkerQueue&) = delete;
    WorkerQueue& operator= (const WorkerQueue&) = delete;
    WorkerQueue(WorkerQueue&&) noexcept{}
    WorkerQueue& operator= (WorkerQueue&&) noexcept{ return *this; }

    void Begin();
    void AddJob(Job* newJob);
    bool IsEmpty() const;
    bool IsRunning() const;
    Job* PopNextTask();
    void WaitForTask();
    void End();
private:
    mutable std::shared_mutex mutex_;
    std::queue<Job*> jobsQueue_;
    std::condition_variable_any conditionVariable_;
    std::atomic<bool> isRunning_{ false };
};

class Worker
{
public:
    Worker(WorkerQueue* queue) : queue_(queue){}
    void Begin();
    void End();
private:
    void Run();
    std::thread thread_;
    WorkerQueue* queue_ = nullptr;
};

static constexpr auto MAIN_QUEUE_INDEX = -1;

class JobSystem
{
public:
    JobSystem();
    ~JobSystem();
    JobSystem(const JobSystem&) = delete;
    JobSystem(JobSystem&&) noexcept = default;
    JobSystem& operator=(const JobSystem&) = delete;
    JobSystem& operator=(JobSystem&&) = default;

    /**
     * @brief SetupNewQueue is a member function that adds a new queue in the JobSystem and
     * adds a certain number of threads attached to it. It must be called before the Begin member function
     */
    int SetupNewQueue(int threadCount = 1);
    /**
     * @brief Begin is a member function that starts the queues and threads of the JobSystem.
     */
    void Begin();
    void AddJob(Job* newJob, int queueIndex = MAIN_QUEUE_INDEX);
    void End();
    void ExecuteMainThread();
private:
    WorkerQueue mainThreadQueue_{};
    std::vector<WorkerQueue> queues_{};
    std::vector<Worker> workers_{};
};

JobSystem* GetJobSystem();
}
#endif //NEKOLIB_JOB_SYSTEM_H
