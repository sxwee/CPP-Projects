#include <iostream>
#include <mutex>
#include <thread>
#include <condition_variable>

using namespace std;

mutex print_mtx;
condition_variable cv;
char flag = 'A';

void func(char ch)
{
    for (int i = 0; i < 10;++i)
    {
        unique_lock<mutex> lk(print_mtx);
        cv.wait(lk, [=](){ return flag == ch; });
        cout << ch;
        flag = (ch == 'C') ? 'A' : ch + 1;
        cv.notify_all();
    }
}

int main()
{
    thread tha(func, 'A');
    thread thb(func, 'B');
    thread thc(func, 'C');
    tha.join();
    thb.join();
    thc.join();
    cout << endl;
    return 0;
}