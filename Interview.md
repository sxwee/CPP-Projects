# 编程语言

**CPP中类中的静态变量为什么需要类外初始化？**

因为类的静态变量是属于类的全局数据，它们在程序执行前就已经被分配了空间，因此不能依靠类实例来完成类静态变量的初始化。

当然若是静态常量，则必须在定义时（类内）进行初始化。

```cpp
class Demo
{
private:
    static int a = 0; // error
    const static int b = 2; // correct
};
```

------

**手撕线程安全的懒汉单例模式**

**单例模式**：保证一个类仅有一个实例，并提供一个访问它的全局访问点，该实例被所有程序模块共享。

**实现思路**：将构造函数设为`private`，以防止外界创建单例类对象，使用类的**私有静态指针变量**指向类的唯一实例，并用一个**公有的静态方法**来获取该实例。

**懒汉与饿汉**：懒汉模式指不用的时候就不去初始化，饿汉模式指在程序运行时就立即初始化。

线程安全的懒汉单例模式实现思路代码如下：

```cpp
#include <iostream>
#include <mutex>
using namespace std;

mutex instance_mtx;

class Single
{
private:
    Single() {};
    static Single* ptr;
public:
    static Single* get_instance()
    {
        if (ptr == nullptr)
        {
            lock_guard<mutex> lk(instance_mtx);
            ptr = new Single();
        }
        return ptr;
    }
};

Single* Single::ptr = nullptr;

int main()
{
    Single* p1 = Single::get_instance();
    Single* p2 = Single::get_instance();
    cout << (p1 == p2) << endl; // 1
    return 0;
}
```

饿汉模式的对应的实现如下:

```cpp
#include <iostream>
using namespace std;

class Single
{
private:
    Single() {};
    static Single* ptr;
public:
    static Single* get_instance()
    {
        return ptr;
    }
};

Single* Single::ptr = new Single();

int main()
{
    Single* p1 = Single::get_instance();
    Single* p2 = Single::get_instance();
    cout << (p1 == p2) << endl; // 1
    return 0;
}
```



# 操作系统



# 计算机网络



# 数据库

