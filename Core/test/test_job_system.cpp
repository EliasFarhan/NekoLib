//
// Created by unite on 22.05.2024.
//

#include <thread/job_system.h>
#include <gtest/gtest.h>


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
    auto parentJob = std::make_shared<neko::FuncJob>( [](){} );
    neko::FuncDependentJob job{parentJob, [](){} };

    EXPECT_FALSE(job.ShouldStart());

    parentJob->Execute();

    EXPECT_TRUE(job.ShouldStart());
    job.Execute();
    EXPECT_TRUE(job.IsDone());
}

TEST(JobSystem, DependenciesJob)
{
    auto parentJob1 = std::make_shared<neko::FuncJob>( [](){} );
    auto parentJob2 = std::make_shared<neko::FuncJob>( [](){} );
    auto parentJob3 = std::make_shared<neko::FuncJob>( [](){} );
    neko::FuncDependenciesJob job{{parentJob1, parentJob2}, [](){} };

    EXPECT_FALSE(job.ShouldStart());

    parentJob1->Execute();
    EXPECT_FALSE(job.ShouldStart());
    parentJob2->Execute();
    EXPECT_TRUE(job.ShouldStart());

    job.AddDependency(parentJob3);
    EXPECT_FALSE(job.ShouldStart());
    parentJob3->Execute();
    EXPECT_TRUE(job.ShouldStart());


    job.Execute();
    EXPECT_TRUE(job.IsDone());
}

TEST(JobSystem, CyclicDependenciesJob)
{
    auto parentJob1 = std::make_shared<neko::FuncDependenciesJob>( [](){} );
    auto parentJob2 = std::make_shared<neko::FuncDependentJob>( parentJob1, [](){} );
    auto parentJob3 = std::make_shared<neko::FuncJob>( [](){} );

    EXPECT_FALSE(parentJob1->AddDependency(parentJob2));
    EXPECT_TRUE(parentJob1->AddDependency(parentJob3));

    auto parentJob4 = std::make_shared<neko::FuncDependenciesJob>(
            std::initializer_list<std::weak_ptr<neko::Job>> {parentJob2},
            [](){} );
    EXPECT_FALSE(parentJob1->AddDependency(parentJob4));

}