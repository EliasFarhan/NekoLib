//
// Created by unite on 19.06.2024.
//

#include <thread/job_system.h>

#include <vector>
#include <thread>
#include <iostream>
#include <array>
#include <numeric>

static constexpr size_t total = 1000;

#ifdef WIN32
#define NOINLINE __declspec(noinline)
#else
#define NOINLINE __attribute__ ((noinline))
#endif

class Wallet
{
private:
    int mMoney = 0;
    NOINLINE void IncMoney();
public:

    [[nodiscard]] int GetMoney() const
    {
        return mMoney;
    }

    void AddMoney(size_t money)
    {
        for (size_t i = 0; i < money; ++i)
        {
            IncMoney();
        }
    }
};
void Wallet::IncMoney()
{
    mMoney++;
}

int TestMultithreadedWallet(int totalThread)
{

    Wallet walletObject;
    neko::JobSystem jobSystem;
    auto queueIndex = jobSystem.SetupNewQueue(totalThread);
    jobSystem.Begin();
    for (int i = 0; i < totalThread; ++i)
    {
        jobSystem.AddJob(std::make_shared<neko::FuncJob>([&walletObject](){
            walletObject.AddMoney(total);
        }), queueIndex);
    }
    jobSystem.End();
    return walletObject.GetMoney();
}

int main()
{

    int val = 0;

    for (int totalThread = 1; totalThread <= 8; totalThread++)
    {
        int errorCount = 0;
        for (size_t k = 0; k < total/totalThread; k++)
        {
            if ((val = TestMultithreadedWallet(totalThread)) != total * totalThread)
            {
                //std::cerr << "Error at count = " << k << " Money in Wallet = " << val << std::endl;
                errorCount++;
            }

        }

        std::cout << "Total error count: " << errorCount << " over " << total/totalThread <<" with threads number : " << totalThread << "\n";
    }
    return 0;
}