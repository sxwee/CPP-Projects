# 项目概述

**简介**：本项目为面试常考多线程算法的C++实现。

# 算法与实现

**题目**：子线程循环 10 次，接着主线程循环 100 次，接着又回到子线程循环 10 次，接着再回到主线程又循环 100 次，如此循环50次，试写出代码。

**解析**：由于需要子线程和主线程间切换，因此可以使用条件变量来完成线程的唤醒与阻塞。

源码实现如下：

```cpp
// sol1.cpp
mutex count_mtx;
condition_variable cv;

int flag = 10;

void func(int num)
{
    for (int i = 0; i < 50; ++i)
    {
        unique_lock<mutex> lk(count_mtx);
        cv.wait(lk, [=]() { return flag == num; });
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
```

------

**题目**：编写一个程序，开启3个线程，这3个线程的ID分别为A、B、C，每个线程将自己的ID在屏幕上打印10遍，要求输出结果必须按`ABC`的顺序显示；如：`ABCABC`依次递推。

**解析**：该题同样也可以采用条件变量来唤醒和阻塞，唤醒和阻塞的条件为当前flag为自己对应的字符。

源码实现如下：

```cpp
// sol2.cpp
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
```

------

**题目**：有四个线程1、2、3、4。线程1的功能就是输出1，线程2的功能就是输出2，以此类推.........现在有四个文件ABCD。初始都为空。现要让四个文件呈如下格式：

```
A：1 2 3 4 1 2....
B：2 3 4 1 2 3....
C：3 4 1 2 3 4....
D：4 1 2 3 4 1....
```

**解析**：创建4个线程，对于每个文件4个线程依次写入对应的数字。

源码实现如下：

```cpp
// sol3.cpp
const int LOOP = 10;

mutex wmtx;
condition_variable cv;

vector<ofstream> files;

int before[] = {4, 1, 2, 3}; // 记录四个文件上次写入的值

void writer(int num)
{
    for (int i = 0; i < LOOP; ++i)
    {
        // 遍历ABCD四个文件
        for (int k = 0; k < 4; ++k)
        {
            unique_lock<mutex> lk(wmtx);
            if (num == 1)
            {
                // 只有之前写入的4才能继续写1
                cv.wait(lk, [&]{ return before[k] == 4; });
                files[k] << 1 << " ";
                before[k] = 1;
                lk.unlock();
                cv.notify_all();
            }
            else if (num == 2)
            {
                // 只有之前写入的1才能继续写2
                cv.wait(lk, [&]{ return before[k] == 1; });
                files[k] << 2 << " ";
                before[k] = 2;
                lk.unlock();
                cv.notify_all();
            }
            else if (num == 3)
            {
                // 只有之前写入的2才能继续写3
                cv.wait(lk, [&]{ return before[k] == 2; });
                files[k] << 3 << " ";
                before[k] = 3;
                lk.unlock();
                cv.notify_all();
            }
            else if (num == 4)
            {
                // 只有之前写入的4才能继续写4
                cv.wait(lk, [&]{ return before[k] == 3; });
                files[k] << 4 << " ";
                before[k] = 4;
                lk.unlock();
                cv.notify_all();
            }
        }
    }
}

int main()
{
    ofstream fa("txt/A.txt", ios_base::app);
    ofstream fb("txt/B.txt", ios_base::app);
    ofstream fc("txt/C.txt", ios_base::app);
    ofstream fd("txt/D.txt", ios_base::app);
    if (!fa.is_open())
    {
        exit(EXIT_FAILURE);
    }
    if (!fb.is_open())
    {
        exit(EXIT_FAILURE);
    }
    if (!fc.is_open())
    {
        exit(EXIT_FAILURE);
    }
    if (!fd.is_open())
    {
        exit(EXIT_FAILURE);
    }
    files.emplace_back(std::move(fa));
    files.emplace_back(std::move(fb));
    files.emplace_back(std::move(fc));
    files.emplace_back(std::move(fd));

    thread th1(writer, 1);
    thread th2(writer, 2);
    thread th3(writer, 3);
    thread th4(writer, 4);

    th1.join();
    th2.join();
    th3.join();
    th4.join();

    for (auto &fp : files)
    {
        fp.close();
    }
    return 0;
}
```

------

**题目**：读者写者问题，有一个写者很多读者，多个读者可以同时读文件，但写者在写文件时不允许有读者在读文件，同样有读者读时写者也不能写。

源码实现如下：

```cpp
// sol4.cpp
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
```

------

**题目**：线程安全的queue。STL中的queue 是非线程安全的，一个组合操作：`front()`、`pop()`先读取队首元素然后删除队首元素，若是有多个线程执行这个组合操作的话，可能会发生执行序列交替执行，导致一些意想不到的行为。因此需要重新设计线程安全的 queue 的接口。

源码实现如下:

```cpp
// sol5.cpp
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
```

------

**题目**：编写程序完成如下功能：

- 有一`int`型全局变量`g_Flag`初始值为0
- 在主线称中起动线程1，打印“this is thread1”，并将`g_Flag`设置为1
- 在主线称中启动线程2，打印“this is thread2”，并将`g_Flag`设置为2
- 线程序1需要在线程2退出后才能退出
- 主线程在检测到`g_Flag`从1变为2，或者从2变为1的时候退出

实现源码如下：

```cpp
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
```



# 参考

[一些c++多线程习题](https://www.cnblogs.com/geloutingyu/p/8513992.html)
