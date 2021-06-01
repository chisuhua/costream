#include "co/coroutine.h"
#include <iostream>
#include <unistd.h>
using namespace std;

void f2()
{
    cout << 2 << endl;
    co_yield;
    cout << 4 << endl;
    co_yield;
    cout << 6 << endl;
}

void f1()
{
    go f2;
    cout << 1 << endl;
    co_yield;
    cout << 3 << endl;
    co_yield;
    cout << 5 << endl;
}

int main()
{
    co_sched.Start(2);
//    g_Scheduler.GetOptions().debug = co::dbg_all;
    go f1;
    cout << "go" << endl;
    //while (!g_Scheduler.IsEmpty()) {
    //    g_Scheduler.Run();
    //}
    //std::thread([]{ co_sleep(200); co_sched.Stop(); }).detach();

    cout << "end" << endl;
    co_sleep(1);
    co_sched.Stop();
    return 0;
}
