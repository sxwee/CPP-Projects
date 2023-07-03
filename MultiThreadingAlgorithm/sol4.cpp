#include <iostream>
#include <thread>
#include <mutex>
#include <condition_variable>
using namespace std;

const int LOOP = 10;

class Semaphore
{
public:
    Semaphore(int cnt = 1) : count(cnt) {}
    // P操作, 减1后信号量若小于0则会阻塞
    void p_op()
    {
        unique_lock<mutex> lk(mtx);
        --count;
        if(count < 0)
            cv.wait(lk);
    }
    // V操作, 加1后若信号量小于等于0则说明之前有线程阻塞, 可以唤醒一个阻塞的线程
    void v_op()
    {
        unique_lock<mutex> lk(mtx);
        ++count;
        if (count <= 0)
            cv.notify_one();
    }

    int get_count()
    {
        return count;
    }

private:
    int count;
    mutex mtx;
    condition_variable cv;
};

Semaphore w_mtx(1); // 控制写操作的互斥信号量
int r_cnt = 0;      // 正在进行读操作的读者个数
Semaphore r_mtx(1); // 控制对r_cnt的互斥修改

void write()
{
    for (int i = 0; i < LOOP; ++i)
    {
        w_mtx.p_op();

        cout << "writing" << endl;

        w_mtx.v_op();
    }
}

void read()
{
    for (int i = 0; i < LOOP; ++i)
    {
        r_mtx.p_op();
        if (r_cnt == 0)
        {
            w_mtx.p_op();
        }
        ++r_cnt;
        r_mtx.v_op();

        cout << "reading" << endl;

        r_mtx.p_op();
        --r_cnt;
        if (r_cnt == 0)
        {
            w_mtx.v_op();
        }
        r_mtx.v_op();
    }
}

int main()
{
    thread tw(write);
    thread tr1(read);
    thread tr2(read);
    thread tr3(read);

    tw.join();
    tr1.join();
    tr2.join();
    tr3.join();

    return 0;
}