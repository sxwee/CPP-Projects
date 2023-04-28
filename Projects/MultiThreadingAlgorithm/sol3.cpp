#include <iostream>
#include <fstream>
#include <thread>
#include <mutex>
#include <vector>
#include <condition_variable>
using namespace std;

const int LOOP = 10;

mutex wmtx;
condition_variable cv;

vector<ofstream> files;

int flags[] = {4, 1, 2, 3}; // 记录四个文件上次写入的值

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
                cv.wait(lk, [&]
                        { return flags[k] == 4; });
                files[k] << num << " ";
                flags[k] = 4 ? 1 : flags[k] + 1;
                lk.unlock();
                cv.notify_all();
            }
            else if (num == 2)
            {
                // 只有之前写入的1才能继续写2
                cv.wait(lk, [&]
                        { return flags[k] == 1; });
                files[k] << 2 << " ";
                flags[k] = 2;
                lk.unlock();
                cv.notify_all();
            }
            else if (num == 3)
            {
                // 只有之前写入的2才能继续写3
                cv.wait(lk, [&]
                        { return flags[k] == 2; });
                files[k] << 3 << " ";
                flags[k] = 3;
                lk.unlock();
                cv.notify_all();
            }
            else if (num == 4)
            {
                // 只有之前写入的4才能继续写4
                cv.wait(lk, [&]
                        { return flags[k] == 3; });
                files[k] << 4 << " ";
                flags[k] = 4;
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