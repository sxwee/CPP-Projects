#include <iostream>
#include <mutex>
#include <condition_variable>
#include <queue>
#include <thread>
#include <exception>

using namespace std;

const int LOOP = 10;

template <typename T>
class threadsafe_queue
{
public:
    threadsafe_queue(){};
    T pop();
    void push(T val);
    bool empty();

private:
    queue<T> q;
    mutex mtx;
    condition_variable cv;
};

template <typename T>
void threadsafe_queue<T>::push(T val)
{
    lock_guard<mutex> lk(mtx);
    q.push(move(val));
    cv.notify_one();
}

template <typename T>
T threadsafe_queue<T>::pop()
{
    unique_lock<mutex> lk(mtx);
    cv.wait(lk, [this]
            { return !q.empty(); });
    T val = move(q.front());
    q.pop();
    return val;
}

template <typename T>
bool threadsafe_queue<T>::empty()
{
    lock_guard<mutex> lk(mtx);
    return q.empty();
}

threadsafe_queue<int> tq;
mutex print_mtx;

void push_value()
{
    for (int i = 0; i < LOOP;++i)
    {
        tq.push(i);
    }
}

void pop_value()
{
    while(true)
    {
        int val = tq.pop();
        unique_lock<mutex> lk(print_mtx);
        cout << "pop: " << val << endl;
    }
}

int main()
{
    thread th1(push_value);
    thread th2(pop_value);
    thread th3(pop_value);

    th1.join();
    th2.join();
    th3.join();
    return 0;
}