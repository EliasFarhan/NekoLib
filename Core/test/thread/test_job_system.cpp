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

TEST(JobSystem, JobSystemSeveralQueuesEmpty)
{

	[[maybe_unused]] int queueIndex = neko::JobSystem::SetupNewQueue(5);

    neko::JobSystem::Begin();


    neko::JobSystem::End();

}

TEST(JobSystem, JobSystemOneQueue)
{

    int queueIndex = neko::JobSystem::SetupNewQueue(1);
    
    neko::JobSystem::Begin();

    int number = 0;
    constexpr int finalNumber = 3;
	neko::FuncJob job([&number, finalNumber]
	{
	    number = finalNumber;
	});
    neko::JobSystem::AddJob(&job, queueIndex);

    neko::JobSystem::End();

    EXPECT_EQ(number, finalNumber);
}

TEST(JobSystem, JobSystemMainQueue)
{

    int queueIndex = neko::JobSystem::SetupNewQueue(1);
    
    neko::JobSystem::Begin();

    constexpr int firstNumber = 1;
    int number = firstNumber;
    constexpr int secondNumber = 3;
	neko::FuncJob firstJob([&number, &firstNumber, secondNumber](){
        EXPECT_EQ(number, firstNumber);
        number = secondNumber;
    });
    neko::JobSystem::AddJob(&firstJob, queueIndex);

    constexpr int finalNumber = 5;
	neko::FuncDependentJob mainJob(&firstJob,
		[&number, &firstNumber, &secondNumber, finalNumber]()
    {
        EXPECT_NE(number, firstNumber);
        EXPECT_EQ(number, secondNumber);
        number = finalNumber;
    });
    neko::JobSystem::AddJob(&mainJob, neko::MAIN_QUEUE_INDEX);
    neko::JobSystem::ExecuteMainThread();
    neko::JobSystem::End();

    EXPECT_EQ(number, finalNumber);
}

TEST(JobSystem, JobSystemDependenciesStart)
{

    int queueIndex = neko::JobSystem::SetupNewQueue(1);

    neko::JobSystem::Begin();

    constexpr int firstNumber = 1;
    int number1 = firstNumber;
    int number2 = firstNumber;
    constexpr int secondNumber = 3;
	neko::FuncJob firstJob([&number1, secondNumber](){

        number1 = secondNumber;
    });
    neko::JobSystem::AddJob(&firstJob, queueIndex);
    constexpr int thirdNumber = 4;
	neko::FuncJob secondJob([&number2, thirdNumber](){

        number2 = thirdNumber;
    });
    neko::JobSystem::AddJob(&secondJob, queueIndex);

    constexpr int finalNumber = 5;
	neko::FuncDependenciesJob mainJob(
            {&firstJob, &secondJob},
            [&number1, &number2, &firstNumber, &secondNumber, &thirdNumber, finalNumber]()
    {
        EXPECT_NE(number1, firstNumber);
        EXPECT_EQ(number1, secondNumber);
        EXPECT_NE(number2, firstNumber);
        EXPECT_EQ(number2, thirdNumber);
        number1 = finalNumber;
        number2 = finalNumber;
    });
    neko::JobSystem::AddJob(&mainJob, neko::MAIN_QUEUE_INDEX);
    neko::JobSystem::ExecuteMainThread();
    neko::JobSystem::End();

    EXPECT_EQ(number1, finalNumber);
    EXPECT_EQ(number2, finalNumber);
}