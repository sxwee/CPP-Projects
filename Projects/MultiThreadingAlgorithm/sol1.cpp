#include <iostream>
#include <thread>
#include <condition_variable>
#include <mutex>

using namespace std;

mutex count_mtx;
condition_variable cv;

int flag = 10;

void func(int num)
{
    for (int i = 0; i < 50; ++i)
    {
        unique_lock<mutex> lk(count_mtx);
        cv.wait(lk, [=]() { return flag != num; });
        for (int j = 0; j < num;++j)
            cout << j << " ";
        cout << endl;
        flag = (num == 10) ? 100 : 10;
        cv.notify_one();
    }
}

int main()
{
    thread child(func, 10);
    func(100);
    child.join();
    return 0;
}