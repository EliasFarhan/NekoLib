//
// Created by unite on 22.05.2024.
//

#include "thread/job_system.h"
#include "gtest/gtest.h"


TEST(JobSystem, FuncJob)
{
    neko::FuncJob job{ [](){} };

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

TEST(JobSystem, DependencyJob)
{
	neko::FuncJob parentJob( [](){} );
    neko::FuncDependentJob job{&parentJob, [](){} };

    EXPECT_FALSE(job.ShouldStart());

    parentJob.Execute();

    EXPECT_TRUE(job.ShouldStart());
    job.Execute();
    EXPECT_TRUE(job.IsDone());
}

TEST(JobSystem, DependenciesJob)
{
    neko::FuncJob parentJob1 ( [](){} );
    neko::FuncJob parentJob2 ( [](){} );
    neko::FuncJob parentJob3 ( [](){} );
    neko::FuncDependenciesJob job{{&parentJob1, &parentJob2}, [](){} };

    EXPECT_FALSE(job.ShouldStart());

    parentJob1.Execute();
    EXPECT_FALSE(job.ShouldStart());
    parentJob2.Execute();
    EXPECT_TRUE(job.ShouldStart());

    job.AddDependency(&parentJob3);
    EXPECT_FALSE(job.ShouldStart());
    parentJob3.Execute();
    EXPECT_TRUE(job.ShouldStart());


    job.Execute();
    EXPECT_TRUE(job.IsDone());
}

TEST(JobSystem, CyclicDependenciesJob)
{
	neko::FuncDependenciesJob parentJob1( [](){} );
	neko::FuncDependentJob parentJob2 ( &parentJob1, [](){} );
	neko::FuncJob parentJob3( [](){} );

    EXPECT_FALSE(parentJob1.AddDependency(&parentJob2));
    EXPECT_TRUE(parentJob1.AddDependency(&parentJob3));

	neko::FuncDependenciesJob parentJob4 (
            std::initializer_list<neko::Job*> {&parentJob2},
            [](){} );
    EXPECT_FALSE(parentJob1.AddDependency(&parentJob4));

}

TEST(JobSystem, WorkerQueue)
{
    neko::WorkerQueue queue;

    queue.Begin();
    EXPECT_TRUE(queue.IsRunning());
    EXPECT_TRUE(queue.IsEmpty());

	neko::FuncJob job([](){});
    queue.AddJob(&job);
    EXPECT_FALSE(queue.IsEmpty());

    auto topJob = queue.PopNextTask();
    EXPECT_EQ(topJob, &job);

    auto noJob = queue.PopNextTask();
    EXPECT_EQ(noJob, nullptr);

    queue.End();
}

TEST(JobSystem, EmptyWorker)
{
    neko::Worker worker(nullptr);
    worker.Begin();

    worker.End();
}

TEST(JobSystem, Worker)
{
    neko::WorkerQueue queue;
    queue.Begin();

    neko::Worker worker(&queue);
    worker.Begin();

    int number = 0;
    constexpr int finalNumber = 3;
	neko::FuncJob job ([&number](){number = finalNumber;});
    queue.AddJob(&job);

    // WorkerQueue must be destroyed before the Worker
    queue.End();
    worker.End();
    EXPECT_EQ(number, finalNumber);
}

TEST(JobSystem, JobSystemSeveralQueuesEmpty)
{
    neko::JobSystem jobSystem;

	[[maybe_unused]] int queueIndex = jobSystem.SetupNewQueue(5);

    jobSystem.Begin();


    jobSystem.End();

}

TEST(JobSystem, JobSystemOneQueue)
{
    neko::JobSystem jobSystem;

    int queueIndex = jobSystem.SetupNewQueue(1);
    
    jobSystem.Begin();

    int number = 0;
    constexpr int finalNumber = 3;
	neko::FuncJob job([&number](){number = finalNumber;});
    jobSystem.AddJob(&job, queueIndex);

    jobSystem.End();

    EXPECT_EQ(number, finalNumber);
}

TEST(JobSystem, JobSystemMainQueue)
{
    neko::JobSystem jobSystem;

    int queueIndex = jobSystem.SetupNewQueue(1);
    
    jobSystem.Begin();

    constexpr int firstNumber = 1;
    int number = firstNumber;
    constexpr int secondNumber = 3;
	neko::FuncJob firstJob([&number, &firstNumber](){
        EXPECT_EQ(number, firstNumber);
        number = secondNumber;
    });
    jobSystem.AddJob(&firstJob, queueIndex);

    constexpr int finalNumber = 5;
	neko::FuncDependentJob mainJob(&firstJob,
		[&number, &firstNumber, &secondNumber]()
    {
        EXPECT_NE(number, firstNumber);
        EXPECT_EQ(number, secondNumber);
        number = finalNumber;
    });
    jobSystem.AddJob(&mainJob, neko::MAIN_QUEUE_INDEX);
    jobSystem.ExecuteMainThread();
    jobSystem.End();

    EXPECT_EQ(number, finalNumber);
}

TEST(JobSystem, JobSystemDependenciesStart)
{
    neko::JobSystem jobSystem;

    int queueIndex = jobSystem.SetupNewQueue(1);

    jobSystem.Begin();

    constexpr int firstNumber = 1;
    int number1 = firstNumber;
    int number2 = firstNumber;
    constexpr int secondNumber = 3;
	neko::FuncJob firstJob([&number1](){

        number1 = secondNumber;
    });
    jobSystem.AddJob(&firstJob, queueIndex);
    constexpr int thirdNumber = 4;
	neko::FuncJob secondJob([&number2](){

        number2 = thirdNumber;
    });
    jobSystem.AddJob(&secondJob, queueIndex);

    constexpr int finalNumber = 5;
	neko::FuncDependenciesJob mainJob(
            {&firstJob, &secondJob},
            [&number1, &number2, &firstNumber, &secondNumber, &thirdNumber]()
    {
        EXPECT_NE(number1, firstNumber);
        EXPECT_EQ(number1, secondNumber);
        EXPECT_NE(number2, firstNumber);
        EXPECT_EQ(number2, thirdNumber);
        number1 = finalNumber;
        number2 = finalNumber;
    });
    jobSystem.AddJob(&mainJob, neko::MAIN_QUEUE_INDEX);
    jobSystem.ExecuteMainThread();
    jobSystem.End();

    EXPECT_EQ(number1, finalNumber);
    EXPECT_EQ(number2, finalNumber);
}