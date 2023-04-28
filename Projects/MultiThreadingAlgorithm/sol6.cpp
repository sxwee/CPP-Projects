#include <iostream>
#include <thread>
#include <mutex>
#include <condition_variable>

using namespace std;

int g_flag = 0;
bool flag[] = {false, false};
mutex mtx;
condition_variable cv;

void worker(int number)
{
    unique_lock<mutex> lk(mtx);
    cout << "This is thread " << number << endl;
    g_flag = number;
    flag[number - 1] = true;
    if (number == 1)
    {
        cv.wait(lk, [&]
                { return g_flag == 2; });
    }
    else if (number == 2)
    {
        cv.notify_all();
    }
    cout << "Thread " << number << " exit!" << endl;
}

int main()
{
    thread th1(worker, 1);
    thread th2(worker, 2);
    th1.detach();
    th2.detach();
    unique_lock<mutex> lk(mtx);
    cv.wait(lk, [&]
            { return flag[0] && flag[1]; });
    cout << "Main thread exit" << endl;
    return 0;
}