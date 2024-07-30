
#include "container/queue.h"
#include <benchmark/benchmark.h>
#include <queue>


const long fromRange = 8;
const long toRange = 1 << 10;

static void BM_PushNekoQueue(benchmark::State& state)
{
    const size_t n = state.range(0);

    for (auto _ : state)
    {
        state.PauseTiming();
        neko::Queue<int> q;
        state.ResumeTiming();

        for(std::size_t i = 0; i < n; i++)
        {
            q.push((int) i);
        }
        benchmark::DoNotOptimize(q.front());
        benchmark::ClobberMemory();
    }
}

BENCHMARK(BM_PushNekoQueue)->Range(fromRange, toRange);

static void BM_PushStdQueue(benchmark::State& state)
{
    const size_t n = state.range(0);

    for (auto _ : state)
    {
        state.PauseTiming();
        std::queue<int> q;
        state.ResumeTiming();

        for(std::size_t i = 0; i < n; i++)
        {
            q.push((int)i);
        }
        benchmark::DoNotOptimize(q.front());
        benchmark::ClobberMemory();
    }
}

BENCHMARK(BM_PushStdQueue)->Range(fromRange, toRange);

static void BM_PopNekoQueue(benchmark::State& state)
{
    const size_t n = state.range(0);

    for (auto _ : state)
    {
        state.PauseTiming();
        neko::Queue<int> q;
        for(std::size_t i = 0; i < n+1; i++)
        {
            q.push((int) i);
        }
        state.ResumeTiming();
        for (std::size_t i = 0; i < n; i++)
        {
            q.pop();
        }

        benchmark::DoNotOptimize(q.front());
        benchmark::ClobberMemory();
    }
}

BENCHMARK(BM_PopNekoQueue)->Range(fromRange, toRange);


static void BM_PopStdQueue(benchmark::State& state)
{
    const size_t n = state.range(0);

    for (auto _ : state)
    {
        state.PauseTiming();
        std::queue<int> q;
        for(std::size_t i = 0; i < n+1; i++)
        {
            q.push((int)i);
        }
        state.ResumeTiming();
        for (std::size_t i = 0; i < n; i++)
        {
            q.pop();
        }

        benchmark::DoNotOptimize(q.front());
        benchmark::ClobberMemory();
    }
}

BENCHMARK(BM_PopStdQueue)->Range(fromRange, toRange);


static void BM_PushAfterPopNekoQueue(benchmark::State& state)
{
    const size_t n = state.range(0);

    for (auto _ : state)
    {
        state.PauseTiming();
        neko::Queue<int> q;
        for(std::size_t i = 0; i < n; i++)
        {
            q.push((int) i);
        }

        for (std::size_t i = 0; i < n/2; i++)
        {
            q.pop();
        }
        for (std::size_t i = 0; i < n/2; i++)
        {
            q.push((int) i);
        }
        state.ResumeTiming();
        // This should trigger a Rearrange
        q.push(12);

        benchmark::DoNotOptimize(q.front());
        benchmark::ClobberMemory();
    }
}

BENCHMARK(BM_PushAfterPopNekoQueue)->Range(fromRange, toRange);


static void BM_PushAfterPopStdQueue(benchmark::State& state)
{
    const size_t n = state.range(0);

    for (auto _ : state)
    {
        state.PauseTiming();
        std::queue<int> q;
        for(std::size_t i = 0; i < n; i++)
        {
            q.push((int)i);
        }
        for (std::size_t i = 0; i < n/2; i++)
        {
            q.pop();
        }
        for(std::size_t i = 0; i < n/2; i++)
        {
            q.push((int)i);
        }
        state.ResumeTiming();
        q.push(12);
        benchmark::DoNotOptimize(q.front());
        benchmark::ClobberMemory();
    }
}

BENCHMARK(BM_PushAfterPopStdQueue)->Range(fromRange, toRange);
// Run the benchmark
BENCHMARK_MAIN();