#ifndef NEKOLIB_JOB_SYSTEM_H
#define NEKOLIB_JOB_SYSTEM_H

#include <vector>
#include <atomic>
#include <functional>
#include <memory>
#include <future>
#include <array>
#include <algorithm>

namespace neko
{

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
private:
    std::atomic<bool> hasStarted_{ false };
    std::atomic<bool> isDone_{ false };
    std::atomic<bool> failed_{ false };
    std::atomic<bool>* cancelFlag_{ nullptr };
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
        if (dependency != nullptr && !dependency->HasStarted())
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

static constexpr auto MAIN_QUEUE_INDEX = -1;

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

private:
    Job* containedJob_;
    int queueIndex_;
    Job* dependency_;
};

namespace JobSystem
{
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
