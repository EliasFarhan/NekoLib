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
#include <cassert>
#include <chrono>
#include <deque>
#include <mutex>


#ifdef TRACY_ENABLE
#include <tracy/Tracy.hpp>
#include <cstdio>
#endif
#include <algorithm>
#include <ranges>

namespace neko
{

namespace
{
/// See JobSystem::SetUnhandledExceptionHandler. Read on every thread that executes a job, written by
/// whoever installs it -- hence atomic, even though it is set once at startup in practice.
std::atomic<JobExceptionHandler> unhandledExceptionHandler_{nullptr};
}

/// The scheduler's window into Job's private state. Deliberately three tiny accessors: a Job carries
/// no scheduling containers, no claim flags and no lock.
///
/// ⚠️ READINESS IS NEVER LATCHED. `ShouldStart()` is re-derived every time it is asked. A dependency
/// counter would be faster and would break two documented behaviours: DependenciesJob::AddDependency
/// can take an already-ready job back to not-ready, and ScheduleJob gates on HasStarted() rather
/// than IsDone().
struct JobScheduler
{
    static void Collect(const Job* job, std::vector<Job*>& out) { job->CollectDependencies(out); }
    static int QueueIndexOf(const Job* job) { return job->queueIndex_.load(std::memory_order_acquire); }
    static void SetQueueIndex(Job* job, int queueIndex)
    {
        job->queueIndex_.store(queueIndex, std::memory_order_release);
    }
};

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
    // ⚠️ THIS CATCH IS WHY AN EXCEPTION NEVER TERMINATES THE PROCESS FROM A JOB. It exists for
    // failure propagation (6818b32): a job that throws is marked failed, and every DependentJob /
    // DependenciesJob downstream of it is skipped as failed instead of running on missing results.
    // So "a worker has no handler above it, an escaping exception is std::terminate" is FALSE here --
    // the exception is caught on every thread, the main queue included.
    //
    // ⚠️ And by itself it is SILENT: failed_ carries no type and no message. That is what the handler
    // is for. It is a process-wide function pointer rather than an exception_ptr member on Job
    // because a Job must stay cheap -- the engine re-submits its frame jobs every frame, and a
    // member would tax every one of them for a path that is taken on a bug.
    try
    {
        ExecuteImpl();
    }
    catch (...)
    {
        failed_.store(true, std::memory_order_release);
        // Before isDone_, so a Join() that returns has already seen the report.
        if (const JobExceptionHandler handler = unhandledExceptionHandler_.load(std::memory_order_acquire);
            handler != nullptr)
        {
            handler(std::current_exception());
        }
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

void Job::CollectDependencies([[maybe_unused]] std::vector<Job*>& out) const
{
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

void DependentJob::CollectDependencies(std::vector<Job*>& out) const
{
    // ⚠️ Reported even when null. DependentJob::ShouldStart() returns FALSE for a null dependency,
    // unlike every other class here, which treats null as ready -- so a null-dependency job can
    // never become ready and AddJob has to be able to see that rather than parking it forever.
    out.push_back(dependency_);
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

void DependenciesJob::CollectDependencies(std::vector<Job*>& out) const
{
    out.insert(out.end(), dependencies_.begin(), dependencies_.end());
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

void ScheduleJob::CollectDependencies(std::vector<Job*>& out) const
{
    if (dependency_ != nullptr)
        out.push_back(dependency_);
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


/// ⚠️ NON-MOVABLE, DELIBERATELY. This used to carry a move constructor with an EMPTY BODY while
/// living in a std::vector: a reallocating SetupNewQueue silently default-constructed a fresh queue
/// in the destination and dropped every pending Job* in the source, and Worker::Run's cached
/// `queues_[i]` reference dangled. Deleting the moves means no container can ever do that again --
/// see the std::deque below, which never needs one.
class WorkerQueue
{
public:
    WorkerQueue() = default;
    WorkerQueue(const WorkerQueue&) = delete;
    WorkerQueue& operator= (const WorkerQueue&) = delete;
    WorkerQueue(WorkerQueue&&) = delete;
    WorkerQueue& operator= (WorkerQueue&&) = delete;

    void AddJob(Job* newJob);
    bool IsEmpty() const;
    Job* PopNextTask();
    /// Blocks until a job (or a shutdown sentinel) arrives. No timeout: an idle worker must cost
    /// nothing at all -- see the ⚠️ on the millisecond variant for why a short one cannot.
    void WaitDequeue(Job*& out);
    /// ⚠️ MILLISECONDS, NOT MICROSECONDS, AND THAT IS THE WHOLE POINT. moodycamel's Windows
    /// semaphore is `WaitForSingleObject(h, usecs / 1000)`, so ANY sub-millisecond timeout truncates
    /// to a 0 ms wait that returns immediately -- turning the caller into a 100% busy loop while
    /// looking like a blocking wait in the source. This queue previously polled at 250 us and burned
    /// one core per idle worker on Windows, permanently, with no job in flight. Unix builds
    /// (including NX) take sem_timedwait and really do sleep, which is exactly why the burn was
    /// invisible to every measurement taken on console.
    bool WaitDequeueFor(Job*& out, std::int64_t timeoutMillis);

    /// Enqueues `newJob` if it is ready, and otherwise parks it in `deferred_` until something
    /// completes. Both halves under one lock, which closes the race where the dependency finishes
    /// between the readiness test and the park.
    void SubmitOrDefer(Job* newJob);
    /// Moves every deferred job that has since become ready into the queue.
    void ReleaseReady();
    [[nodiscard]] int DeferredCount() const { return deferredCount_.load(std::memory_order_acquire); }

private:
    moodycamel::BlockingConcurrentQueue<Job*> jobsQueue_;
    /// ⚠️ THE NOT-YET-READY SET LIVES HERE, NOT ON Job. It is empty outside a level load and holds
    /// three entries during one, so it is a small vector behind a mutex that is almost never taken.
    /// Putting a dependents list (and a lock) on every Job instead would tax the seven frame jobs
    /// the engine re-submits every frame, for a set that is empty whenever the game is running.
    std::mutex deferredMutex_;
    std::vector<Job*> deferred_;
    /// Read without the lock, by the shutdown drain and by the global fast path.
    std::atomic<int> deferredCount_{ 0 };
};




class Worker
{
public:
    Worker(std::size_t queueIndex, std::size_t workerIndex)
        : queueIndex_(queueIndex), workerIndex_(workerIndex){}
    void Begin();
    void End();
private:
    void Run() const;
    std::thread thread_;
    std::size_t queueIndex_ = std::numeric_limits<size_t>::max();
    // Only used to build this worker's profiler thread name, so it need not be globally unique --
    // it is an ordinal within the queue.
    //
    // ⚠️ [[maybe_unused]] is load-bearing, not decoration: this field is read ONLY inside an
    // #ifdef TRACY_ENABLE block, and NekoCore compiles with -Werror on every non-MSVC toolchain, so
    // without it a plain (non-profiling) clang build fails on -Wunused-private-field.
    [[maybe_unused]] std::size_t workerIndex_ = 0;
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
/// ⚠️ A REAL OBJECT, NOT nullptr. Shutdown wakes each blocked worker by enqueuing a sentinel, and
/// `PopNextTask()` already returns nullptr to mean "queue empty" -- so a nullptr sentinel is
/// indistinguishable from an empty queue. A draining worker then silently ate a peer's wake-up and
/// that peer blocked in wait_dequeue forever, hanging JobSystem::End(). Caught by
/// test_job_system.cpp's JobSystemSeveralQueuesEmpty, which is the only thing in the tree that
/// starts several workers and immediately shuts them down.
class ShutdownSentinelJob final : public Job
{
protected:
    void ExecuteImpl() override {}
};
ShutdownSentinelJob shutdownSentinel_{};

WorkerQueue mainThreadQueue_{};
// ⚠️ std::deque, NOT std::vector. Worker::Run caches `queues_[queueIndex_]` by reference for the
// whole thread lifetime and Worker::Begin captures `this` into its thread; a vector reallocation
// dangles both. A deque never relocates existing elements on push_back, so both stay valid, and
// WorkerQueue does not need the move constructor that used to lose jobs.
std::deque<WorkerQueue> queues_{};
std::deque<Worker> workers_{};
/// Workers per queue, so shutdown can hand each one its own wake-up sentinel.
std::vector<int> queueWorkerCounts_{};
std::atomic<bool> isRunning_{ false };
/// ⚠️ THE WHOLE STEADY-STATE COST OF THE CONTINUATION MODEL. Deferral only ever happens on a worker
/// queue during a scene load, so outside one this is 0 and ReleaseReady() is a single relaxed load
/// -- no lock, no scan, nothing touched on the per-frame path.
std::atomic<int> totalDeferred_{ 0 };
}

void ReleaseReady()
{
    if (totalDeferred_.load(std::memory_order_relaxed) == 0)
        return;
    for (auto& queue : queues_)
    {
        queue.ReleaseReady();
    }
}

Job* ShutdownSentinel() { return &shutdownSentinel_; }

void SetUnhandledExceptionHandler(JobExceptionHandler handler)
{
    unhandledExceptionHandler_.store(handler, std::memory_order_release);
}

int SetupNewQueue(int threadCount)
{
    // The "call me before Begin()" rule was a doc comment with nothing enforcing it, while breaking
    // it means spawning a worker onto a queue that does not exist yet.
    assert(!isRunning_.load(std::memory_order_acquire) &&
           "JobSystem::SetupNewQueue must be called before JobSystem::Begin");
    const int newQueueIndex = static_cast<int>(queues_.size());
    queues_.emplace_back();
    queueWorkerCounts_.push_back(threadCount);
    for(int i = 0; i < threadCount; i++)
    {
        workers_.emplace_back(static_cast<std::size_t>(newQueueIndex), static_cast<std::size_t>(i));
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
    // ⚠️ Reset() stays here, on the SUBMITTING thread, ordered before the job becomes visible to any
    // worker. The editor's TaskManager re-submits the same Task at every main<->worker hop and gates
    // the hop on IsDone(); if the enqueue were visible before the reset, it would read the previous
    // phase's isDone_ and advance a phase early.
    newJob->Reset();
    JobScheduler::SetQueueIndex(newJob, queueIndex);

    // ⚠️ THE MAIN QUEUE KEEPS IMMEDIATE ENQUEUE, and that asymmetry is deliberate. It has exactly one
    // consumer, so ExecuteMainThread() can afford to block on an unmet dependency (it does; see
    // below) and callers rely on a single ExecuteMainThread() call running a chain to completion.
    // Deferring here would mean a submitted main-queue job is invisible to the drain that follows it.
    if(queueIndex == MAIN_QUEUE_INDEX)
    {
        mainThreadQueue_.AddJob(newJob);
        return;
    }

    // A not-ready job is parked, never queued: a worker that popped one could only put it back, and
    // that churn is a full-core busy loop which also makes the shutdown drain non-terminating.
    queues_[queueIndex].SubmitOrDefer(newJob);
}

void End()
{
    // ⚠️ Ordered first, and it is what makes the shutdown contract work: every AddJob the caller made
    // before End() is sequenced before this release store, which each worker's acquire load
    // synchronises with, so the drain below is guaranteed to see those jobs.
    isRunning_.store(false, std::memory_order_release);
    // Workers block indefinitely now, so they will not notice isRunning_ on their own. Hand each one
    // a null sentinel to wake on.
    for(std::size_t i = 0; i < queues_.size(); ++i)
    {
        const int workerCount = i < queueWorkerCounts_.size() ? queueWorkerCounts_[i] : 0;
        for(int w = 0; w < workerCount; ++w)
        {
            queues_[i].AddJob(ShutdownSentinel());
        }
    }
    for(auto& worker: workers_)
    {
        worker.End();
    }
    queues_.clear();
    workers_.clear();
    queueWorkerCounts_.clear();
    // ⚠️ The main queue was never cleared here. Anything left on it survived into the next Begin()
    // session -- and since jobs are routinely stack locals, a later ExecuteMainThread() would pop a
    // dangling `this`.
    while(mainThreadQueue_.PopNextTask() != nullptr)
    {
    }
}

void ExecuteMainThread()
{
    // A job that is not ready used to be re-queued into the very queue this loop is draining, with a
    // yield() -- an unbounded 100% busy-wait on the main thread until the dependency landed. The vk
    // backend measured it at ~300 ms of blocked frame on one scene. Blocking on the dependency costs
    // the same latency and no CPU.
    //
    // Callers depend on ONE call running a whole chain to completion, so this must not simply defer.
    std::vector<Job*> dependencies;
    // Only trips on a dependency cycle or a job whose ShouldStart() reads something
    // CollectDependencies() does not report; both are bugs, and spinning on them forever is a worse
    // way to find out.
    constexpr int kMaxConsecutiveRequeues = 4096;
    int consecutiveRequeues = 0;
    // ⚠️ Also a safety net for a Job executed outside the worker loop -- the editor's TaskManager
    // runs main-affinity phases by calling Execute() directly, which reaches no ReleaseReady().
    ReleaseReady();
    while (auto newTask = mainThreadQueue_.PopNextTask())
    {
        if (newTask->ShouldStart())
        {
            consecutiveRequeues = 0;
            newTask->Execute();
            // ⚠️ Load-bearing: GpuSetupJob runs on THIS queue while FinalizeJob sits deferred on the
            // scene-load queue waiting for it. Without this the load never completes.
            ReleaseReady();
            continue;
        }

        dependencies.clear();
        JobScheduler::Collect(newTask, dependencies);
        // ⚠️ A dependency that also lives on the MAIN queue must NOT be joined: this thread is its
        // only consumer, so waiting on it here deadlocks. Put the job back and keep draining -- FIFO
        // submission order means the dependency is ahead of it and this does not normally happen.
        const bool waitsOnThisQueue =
            std::any_of(dependencies.begin(), dependencies.end(),
                        [](const Job* dep)
                        {
                            return dep != nullptr && !dep->IsDone() &&
                                   JobScheduler::QueueIndexOf(dep) == MAIN_QUEUE_INDEX;
                        });
        if (waitsOnThisQueue)
        {
            mainThreadQueue_.AddJob(newTask);
            if (++consecutiveRequeues > kMaxConsecutiveRequeues)
            {
                assert(false && "main-queue job never became ready; dependency cycle or unreported dependency");
                break;
            }
            continue;
        }

        for (Job* dependency : dependencies)
        {
            if (dependency != nullptr)
                dependency->Join();
        }
        consecutiveRequeues = 0;
        if (!newTask->ShouldStart())
        {
            // Every dependency it named has finished and it still refuses to start, so nothing here
            // can ever make it run (a null-dependency DependentJob). Dropping it beats the old
            // behaviour, which was to spin this thread forever.
            assert(false && "main-queue job not ready after joining every dependency it reported");
            continue;
        }
        newTask->Execute();
        ReleaseReady();
    }
}


}
void Worker::Run() const
{
#ifdef TRACY_ENABLE
    // Tracy copies the string, so a local buffer is fine.
    //
    // Without this every worker appears in the timeline as a bare numeric thread id. That is merely
    // inconvenient on desktop, where the sampling profiler and the context-switch view can identify
    // a thread by what it is doing -- but on Nintendo Switch, where TRACY_NO_SAMPLING,
    // TRACY_NO_CALLSTACK and TRACY_NO_CONTEXT_SWITCH are all forced (no backends exist), an unnamed
    // thread is genuinely unidentifiable.
    char threadName[32];
    std::snprintf(threadName, sizeof(threadName), "Worker q%zu/%zu", queueIndex_, workerIndex_);
    tracy::SetThreadName(threadName);
#endif
    // Reference into a std::deque, so it stays valid even if another queue is added later.
    auto& queue = JobSystem::queues_[queueIndex_];
    Job* const sentinel = JobSystem::ShutdownSentinel();
    // ⚠️ Every sentinel this worker takes out of the queue is put back before it exits (see the end
    // of this function). Consuming one without replacing it strands whichever peer was going to wake
    // on it -- and one worker can easily take several, because the drain below pops whatever is
    // there. Putting them all back keeps the count at "one per worker" no matter who took what.
    int sentinelsTaken = 0;

    while(true)
    {
        Job* newTask = nullptr;
        // Blocks. An idle worker costs nothing -- no timeout to truncate, no spin.
        queue.WaitDequeue(newTask);
        if (newTask == sentinel)
        {
            ++sentinelsTaken;
            break;
        }
        if (newTask == nullptr)
        {
            continue;
        }
        // Under the continuation model a job only reaches a queue once it is ready, so the old
        // re-queue-and-yield branch is gone. A job that got here not-ready would be a scheduler bug.
        assert(newTask->ShouldStart() && "a not-ready job reached a worker queue");
        newTask->Execute();
        // Whatever was waiting on it can go now. One relaxed atomic load when nothing is deferred.
        JobSystem::ReleaseReady();
    }

    // Shutdown drain: anything already queued must still run (JobSystem::End() is the completion
    // barrier callers rely on). ⚠️ Waiting for the queue to LOOK empty is not enough -- a job that is
    // merely deferred is not in the queue at all, and the dependency that will release it may be
    // running on another worker right now. Wait the deferred count out too.
    //
    // The deadline exists because a deferred job whose dependency was stranded (never submitted, or
    // submitted to a queue that already drained) would otherwise hang End() forever, exactly as the
    // old re-queue loop did.
    constexpr auto kDrainDeadline = std::chrono::seconds(5);
    const auto drainStart = std::chrono::steady_clock::now();
    while (true)
    {
        Job* newTask = queue.PopNextTask();
        if (newTask == sentinel)
        {
            ++sentinelsTaken;
            continue;
        }
        if (newTask == nullptr)
        {
            // Nothing else will call ReleaseReady() for us once the pool is winding down.
            queue.ReleaseReady();
            if (queue.DeferredCount() <= 0)
                break;
            if (std::chrono::steady_clock::now() - drainStart > kDrainDeadline)
            {
                assert(false && "shutdown drain timed out: a deferred job's dependency never ran");
                break;
            }
            // Milliseconds, never microseconds -- see WorkerQueue::WaitDequeueFor.
            queue.WaitDequeueFor(newTask, 1);
            if (newTask == sentinel)
            {
                ++sentinelsTaken;
            }
            continue;
        }
        if (newTask->ShouldStart())
        {
            newTask->Execute();
            JobSystem::ReleaseReady();
        }
    }

    for (int i = 0; i < sentinelsTaken; ++i)
    {
        queue.AddJob(sentinel);
    }
}


void WorkerQueue::SubmitOrDefer(Job* newJob)
{
    std::scoped_lock lock(deferredMutex_);
    if (newJob->ShouldStart())
    {
        jobsQueue_.enqueue(newJob);
        return;
    }
    // Nothing here can ever become ready on its own -- in this tree that is only a DependentJob
    // built with a null dependency, whose ShouldStart() is false for a null where every other class
    // treats null as ready. It never ran before either; the difference is that it no longer burns a
    // core forever, and no longer wedges JobSystem::End().
#ifndef NDEBUG
    {
        std::vector<Job*> dependencies;
        JobScheduler::Collect(newJob, dependencies);
        assert(std::any_of(dependencies.begin(), dependencies.end(),
                           [](const Job* dep) { return dep != nullptr; }) &&
               "job is not ready and has no dependency that could ever make it ready");
    }
#endif
    deferred_.push_back(newJob);
    deferredCount_.fetch_add(1, std::memory_order_acq_rel);
    JobSystem::totalDeferred_.fetch_add(1, std::memory_order_acq_rel);
}

void WorkerQueue::ReleaseReady()
{
    std::scoped_lock lock(deferredMutex_);
    if (deferred_.empty())
        return;
    // ⚠️ ShouldStart() is asked here and nowhere else. It is not cached, so a dependency added after
    // submission still takes a job back out of contention, which DependenciesJob permits.
    const auto ready = std::partition(deferred_.begin(), deferred_.end(),
                                      [](const Job* job) { return !job->ShouldStart(); });
    for (auto it = ready; it != deferred_.end(); ++it)
    {
        jobsQueue_.enqueue(*it);
    }
    const auto released = static_cast<int>(std::distance(ready, deferred_.end()));
    if (released == 0)
        return;
    deferred_.erase(ready, deferred_.end());
    deferredCount_.fetch_sub(released, std::memory_order_acq_rel);
    JobSystem::totalDeferred_.fetch_sub(released, std::memory_order_acq_rel);
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

void WorkerQueue::WaitDequeue(Job*& out)
{
    jobsQueue_.wait_dequeue(out);
}

bool WorkerQueue::WaitDequeueFor(Job*& out, std::int64_t timeoutMillis)
{
    // ⚠️ Guarding the trap that caused this whole rewrite: moodycamel's Windows semaphore does
    // `WaitForSingleObject(h, usecs / 1000)`, so anything under 1000 us becomes a 0 ms wait that
    // returns instantly and turns the caller into a busy loop.
    assert(timeoutMillis >= 1 && "sub-millisecond waits truncate to a 0 ms poll on Windows");
    return jobsQueue_.wait_dequeue_timed(out, timeoutMillis * 1000);
}
}
