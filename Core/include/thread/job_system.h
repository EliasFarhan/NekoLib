#ifndef NEKOLIB_JOB_SYSTEM_H
#define NEKOLIB_JOB_SYSTEM_H

#include <vector>
#include <atomic>
#include <exception>
#include <functional>
#include <memory>
#include <future>
#include <array>
#include <algorithm>

namespace neko
{

static constexpr auto MAIN_QUEUE_INDEX = -1;

/// Scheduler internals, so the continuation machinery below can reach Job's private state without
/// exposing it as public API. Defined in job_system.cpp; there is no instance of it.
struct JobScheduler;

class Job
{
public:
	Job() = default;
    virtual ~Job() = default;
    virtual void Execute();
    [[nodiscard]] bool HasStarted() const;
    [[nodiscard]] bool IsDone() const;
    [[nodiscard]] bool HasFailed() const;
    [[nodiscard]] bool IsCancelled() const;
    [[nodiscard]] virtual bool ShouldStart() const;
    void Reset();
    void Join() const;
    void SetCancelFlag(std::atomic<bool>* flag) { cancelFlag_ = flag; }

    /**
     * \brief CheckDependency is a member function used to check if the arg ptr is already a dependency
     * @param ptr
     * @return false if not a dependency
     */
    virtual bool CheckDependency(const Job* ptr) const;

protected:
    virtual void ExecuteImpl() = 0;
    void SkipAsFailed();
    void MarkStarted();
    void MarkDone();
    void MarkFailed();

    /// Reports the jobs this one waits on, so the scheduler can register for a wake-up instead of
    /// polling ShouldStart(). The DEFAULT IS EMPTY, which is correct for a Job whose ShouldStart()
    /// is unconditional.
    ///
    /// ⚠️ This is an INDEX, not the readiness rule. `ShouldStart()` remains the only authority on
    /// whether a job may run -- the scheduler re-derives readiness by calling it, and never latches
    /// the answer. That is what keeps `AddDependency()` able to move a job back from ready to
    /// not-ready (test_job_system.cpp's DependenciesJob case) and what lets a subclass gate on
    /// HasStarted() rather than IsDone().
    ///
    /// ⚠️ Every dependency reported here MUST be reachable from ShouldStart(), or the job is
    /// enqueued before it is ready. Every dependency ShouldStart() reads MUST be reported here, or
    /// the job is never woken at all.
    virtual void CollectDependencies(std::vector<Job*>& out) const;

private:
    friend struct JobScheduler;

    std::atomic<bool> hasStarted_{ false };
    std::atomic<bool> isDone_{ false };
    std::atomic<bool> failed_{ false };
    std::atomic<bool>* cancelFlag_{ nullptr };

    /// Which queue this job was last submitted to. The ONLY state the scheduler keeps on a Job --
    /// everything else it needs lives on the queue, because that is where the (tiny, usually empty)
    /// set of not-yet-ready jobs belongs. A Job must stay cheap: the engine re-submits the same
    /// seven of them every single frame.
    std::atomic<int> queueIndex_{ MAIN_QUEUE_INDEX };
};


class DependentJob : public Job
{
public:
    DependentJob(Job* dependency) : dependency_(dependency)
    {

    }
    void Execute() override;
    [[nodiscard]] bool ShouldStart() const override;
	[[nodiscard]] bool CheckDependency(const Job *ptr) const override;
protected:
    void CollectDependencies(std::vector<Job*>& out) const override;
private:
    Job* dependency_{};
};

class DependenciesJob: public Job
{
public:
    DependenciesJob() = default;
    DependenciesJob(std::initializer_list<Job*> dependencies) : dependencies_(dependencies){}
    [[nodiscard]] bool ShouldStart() const override;
    bool AddDependency(Job* dependency);
    void Execute() override;
protected:
    bool CheckDependency(const Job *ptr) const override;
    void CollectDependencies(std::vector<Job*>& out) const override;
    std::vector<Job*> dependencies_{};
};

template<size_t N>
class FixedDependenciesJob : Job
{
public:
    bool AddDependency(Job* dependency);
    void Execute() override;
    bool ShouldStart() const override;
protected:
    bool CheckDependency(const Job *ptr) const override;
    void CollectDependencies(std::vector<Job*>& out) const override;
    std::array<Job*, N> dependencies_{};
};


template<size_t N>
bool FixedDependenciesJob<N>::AddDependency(Job* dependency)
{
    if (dependency == nullptr || dependency->CheckDependency(this))
    {
        return false;
    }
    auto it = std::find(dependencies_.begin(), dependencies_.end(), nullptr);
    if (it != dependencies_.end())
    {
        *it = dependency;
        return true;
    }
    return false;
}

template<size_t N>
void FixedDependenciesJob<N>::Execute()
{
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

template<size_t N>
bool FixedDependenciesJob<N>::ShouldStart() const
{
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

template<size_t N>
bool FixedDependenciesJob<N>::CheckDependency(const Job* ptr) const
{
    return std::any_of(dependencies_.begin(), dependencies_.end(), [ptr](const auto* dep){
        if (dep == nullptr)
            return false;
        return dep->CheckDependency(ptr);
    });
}

/// Dynamically adds a contained job to a target queue once its own dependency
/// has finished.  Useful when a job must run on a specific queue (e.g. the main
/// thread) but should NOT be pre-scheduled — avoiding the wasted per-frame
/// re-queue churn while it sits waiting for upstream work.
///
/// Robust under cancellation: the contained job is ALWAYS scheduled, even if
/// this job's dependency fails or this job itself is cancelled.  The contained
/// job's own cancel flag / failure propagation is responsible for end-to-end
/// cancellation semantics; without this guarantee, any downstream job joining
/// on the contained job would deadlock.
class ScheduleJob : public Job
{
public:
    ScheduleJob(Job* containedJob, int queueIndex, Job* dependency = nullptr)
        : containedJob_(containedJob), queueIndex_(queueIndex), dependency_(dependency) {}

    void Execute() override;
    [[nodiscard]] bool ShouldStart() const override;
    [[nodiscard]] bool CheckDependency(const Job* ptr) const override;

protected:
    void ExecuteImpl() override {}
    void CollectDependencies(std::vector<Job*>& out) const override;

private:
    Job* containedJob_;
    int queueIndex_;
    Job* dependency_;
};

/// Receives the exception that escaped a job's ExecuteImpl(). Runs on whichever thread executed the
/// job -- a worker, or the main thread inside ExecuteMainThread() -- so it must be thread-safe, and
/// it must not throw: it is called from inside Job::Execute's catch block.
using JobExceptionHandler = void (*)(std::exception_ptr exception) noexcept;

namespace JobSystem
{
    /**
     * @brief SetUnhandledExceptionHandler installs the process-wide handler Job::Execute calls when
     * an exception escapes a job. nullptr (the default) removes it.
     *
     * ⚠️ The job is marked failed EITHER WAY -- the handler adds a report, it does not replace the
     * failure. Without one, an escaping exception becomes `HasFailed()` and nothing else: its type
     * and message are gone, and nothing is printed.
     */
    void SetUnhandledExceptionHandler(JobExceptionHandler handler);
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

};

}
#endif //NEKOLIB_JOB_SYSTEM_H
