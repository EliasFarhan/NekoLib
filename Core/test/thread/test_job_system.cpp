//
// Created by unite on 22.05.2024.
//

#include "thread/job_system.h"
#include "gtest/gtest.h"

class EmptyJob : public neko::Job
{
    void ExecuteImpl() override {}
};

class ControlledJob : public EmptyJob
{
public:
    void SetStarted()
    {
        MarkStarted();
    }

    void SetDone()
    {
        MarkDone();
    }
};

TEST(JobSystem, FuncJob)
{
    EmptyJob job{};

    EXPECT_FALSE(job.HasStarted());
    EXPECT_FALSE(job.IsDone());
    EXPECT_TRUE(job.ShouldStart());

    job.Execute();

    EXPECT_TRUE(job.IsDone());
    EXPECT_TRUE(job.HasStarted());
    EXPECT_TRUE(job.ShouldStart());

    job.Reset();

    EXPECT_FALSE(job.HasStarted());
    EXPECT_FALSE(job.IsDone());
    EXPECT_TRUE(job.ShouldStart());
}

class EmptyDependentJob : public neko::DependentJob
{
    using DependentJob::DependentJob;
    void ExecuteImpl() override {}
};

TEST(JobSystem, DependencyJob)
{
	ControlledJob parentJob;
    EmptyDependentJob job{&parentJob};

    EXPECT_FALSE(job.ShouldStart());

    parentJob.SetStarted();
    EXPECT_FALSE(job.ShouldStart());

    parentJob.SetDone();
    EXPECT_TRUE(job.ShouldStart());
    job.Execute();
    EXPECT_TRUE(job.IsDone());
}
class EmptyDependenciesJob : public neko::DependenciesJob
{
    using DependenciesJob::DependenciesJob;
    void ExecuteImpl() override {}
};
TEST(JobSystem, DependenciesJob)
{
    ControlledJob parentJob1;
    ControlledJob parentJob2;
    ControlledJob parentJob3;
    EmptyDependenciesJob job{{&parentJob1, &parentJob2}};

    EXPECT_FALSE(job.ShouldStart());

    parentJob1.SetStarted();
    EXPECT_FALSE(job.ShouldStart());
    parentJob1.SetDone();
    EXPECT_FALSE(job.ShouldStart());
    parentJob2.SetStarted();
    EXPECT_FALSE(job.ShouldStart());
    parentJob2.SetDone();
    EXPECT_TRUE(job.ShouldStart());

    job.AddDependency(&parentJob3);
    EXPECT_FALSE(job.ShouldStart());
    parentJob3.SetStarted();
    EXPECT_FALSE(job.ShouldStart());
    parentJob3.SetDone();
    EXPECT_TRUE(job.ShouldStart());


    job.Execute();
    EXPECT_TRUE(job.IsDone());
}

TEST(JobSystem, CyclicDependenciesJob)
{
	EmptyDependenciesJob parentJob1;
	EmptyDependentJob parentJob2 (&parentJob1);
	EmptyJob parentJob3;

    EXPECT_FALSE(parentJob1.AddDependency(&parentJob2));
    EXPECT_TRUE(parentJob1.AddDependency(&parentJob3));

	EmptyDependenciesJob parentJob4 (std::initializer_list<neko::Job*> {&parentJob2});
    EXPECT_FALSE(parentJob1.AddDependency(&parentJob4));

}

TEST(JobSystem, JobSystemSeveralQueuesEmpty)
{

	[[maybe_unused]] int queueIndex = neko::JobSystem::SetupNewQueue(5);

    neko::JobSystem::Begin();


    neko::JobSystem::End();

}

template<int finalNumber>
class AssignementJob : public neko::Job
{
public:
    explicit AssignementJob(int& number): number_(number){}
    void ExecuteImpl() override
    {
        number_ = finalNumber;
    }
private:
    int& number_;
};

TEST(JobSystem, JobSystemOneQueue)
{

    int queueIndex = neko::JobSystem::SetupNewQueue(1);
    
    neko::JobSystem::Begin();

    int number = 0;
    constexpr int finalNumber = 3;
	AssignementJob<finalNumber> job(number);
    neko::JobSystem::AddJob(&job, queueIndex);

    neko::JobSystem::End();

    EXPECT_EQ(number, finalNumber);
}

template<int finalNumber, int expectedNumber>
class ExpectedAssignmentJob : public neko::Job
{
public:
    explicit ExpectedAssignmentJob(int& number): number_(number){}
    void ExecuteImpl() override
    {
        EXPECT_EQ(number_, expectedNumber);
        number_ = finalNumber;
    }
private:
    int& number_;
};

template<int finalNumber, int expectedNumber>
class DependentExpectedAssignmentJob: public neko::DependentJob
{
public:
    explicit DependentExpectedAssignmentJob(Job* parentJob, int& number): number_(number), DependentJob(parentJob){}
    void ExecuteImpl() override
    {
        EXPECT_EQ(number_, expectedNumber);
        number_ = finalNumber;
    }
    private:
    int& number_;
};

TEST(JobSystem, JobSystemMainQueue)
{

    int queueIndex = neko::JobSystem::SetupNewQueue(1);
    
    neko::JobSystem::Begin();

    constexpr int firstNumber = 1;
    int number = firstNumber;
    constexpr int secondNumber = 3;
	ExpectedAssignmentJob<secondNumber, firstNumber> firstJob(number);
    neko::JobSystem::AddJob(&firstJob, queueIndex);

    constexpr int finalNumber = 5;
	DependentExpectedAssignmentJob<finalNumber, secondNumber> mainJob(&firstJob,
		number);
    neko::JobSystem::AddJob(&mainJob, neko::MAIN_QUEUE_INDEX);
    neko::JobSystem::ExecuteMainThread();
    neko::JobSystem::End();

    EXPECT_EQ(number, finalNumber);
}

template<int finalNumber, int expectedNumber1, int expectedNumber2>
class DependenciesExpectedAssignmentJob : public neko::DependenciesJob
{
public:
    DependenciesExpectedAssignmentJob(std::initializer_list<Job*> parentJobs, int& number1, int& number2): number1_(number1), number2_(number2), DependenciesJob(parentJobs){}
    void ExecuteImpl() override
    {
        EXPECT_EQ(number1_, expectedNumber1);
        EXPECT_EQ(number2_, expectedNumber2);
        number1_ = finalNumber;
        number2_ = finalNumber;
    }
private:
    int& number1_;
    int& number2_;
};

TEST(JobSystem, JobSystemDependenciesStart)
{

    int queueIndex = neko::JobSystem::SetupNewQueue(1);

    neko::JobSystem::Begin();

    constexpr int firstNumber = 1;
    int number1 = firstNumber;
    int number2 = firstNumber;
    constexpr int secondNumber = 3;
	AssignementJob<secondNumber> firstJob(number1);
    neko::JobSystem::AddJob(&firstJob, queueIndex);
    constexpr int thirdNumber = 4;
	AssignementJob<thirdNumber> secondJob(number2);
    neko::JobSystem::AddJob(&secondJob, queueIndex);

    constexpr int finalNumber = 5;
	DependenciesExpectedAssignmentJob<finalNumber, secondNumber, thirdNumber> mainJob(
            {&firstJob, &secondJob},
            number1, number2);

    neko::JobSystem::AddJob(&mainJob, neko::MAIN_QUEUE_INDEX);
    neko::JobSystem::ExecuteMainThread();
    neko::JobSystem::End();

    EXPECT_EQ(number1, finalNumber);
    EXPECT_EQ(number2, finalNumber);
}

TEST(JobSystem, ScheduleJobStartsWhenDependencyHasStarted)
{
    ControlledJob parentJob;
    EmptyJob containedJob;
    neko::ScheduleJob job{&containedJob, neko::MAIN_QUEUE_INDEX, &parentJob};

    EXPECT_FALSE(job.ShouldStart());

    parentJob.SetStarted();

    EXPECT_TRUE(job.ShouldStart());
}

TEST(JobSystem, ScheduleJobCancellationDoesNotDeadlockContainedJob)
{
    int queueIndex = neko::JobSystem::SetupNewQueue(1);
    std::atomic<bool> cancelFlag = true;
    EmptyJob containedJob;
    containedJob.SetCancelFlag(&cancelFlag);
    neko::ScheduleJob scheduleJob{&containedJob, queueIndex};
    scheduleJob.SetCancelFlag(&cancelFlag);

    neko::JobSystem::Begin();
    neko::JobSystem::AddJob(&scheduleJob, queueIndex);

    scheduleJob.Join();
    containedJob.Join();
    neko::JobSystem::End();

    EXPECT_TRUE(scheduleJob.IsDone());
    EXPECT_TRUE(scheduleJob.HasFailed());
    EXPECT_TRUE(containedJob.IsDone());
    EXPECT_TRUE(containedJob.HasFailed());
}
