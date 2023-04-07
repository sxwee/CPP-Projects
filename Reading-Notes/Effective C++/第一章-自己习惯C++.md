# 条款1： 视C++为一个语言联邦

可以将C++视为**由相关语言组成的联邦、而非单一语言**，C++中主要的次语言包括：

- **C part of C++**: C++是由C扩展而来的，其没有模板、没有异常、没有重载。
- **Object-Oriented C++**: 面向对象编程的部分、内若包括类、封装、继承、多态等等。
- **Template C++**: 范型编程
- **STL**: 范型编程的代表作，是一个模板库，其内定义了一系列容器、算法、迭代器等内容。

在C++中，当从一个次语言切换到另一个次语言时，导致高效编程的守则可能会改变。例如对内置（C-like）类型，pass-by-value通常比pass-by-reference高效，当当从C part of C++迁移到Object-Oriented C++，由于用户自定义的构造函数和析构函数的存在，pass-by-reference-to-const往往更好。

> 在64位系统中，参数是通过寄存器来传递的，若按值传递，参数甚至就在寄存器上，直接就能用，而若按地址的话，则还需要一次取地址操作才能获取实际的值。

**总结：C++高效编程守则视情况而定，取决于你使用C++的那一部分**。



# 条框2：尽量以`const`、`enum`、`inline`替换`#define`

`#define`不被视为语言的一部分，使用它会带来许多问题。

**问题一**：当运用宏定义的常量但获得一个编译错误时可能你会无法追踪，因为宏定义的名称可能没有进入记号表（symbol table）。

> 更好的解决方案是使用`const`定义常量来代替宏定义。

**问题二**：使用宏可以实现类似函数的功能，这样使得它没有函数调用带来的额外开销，但这样也会带来一些麻烦，例如下述代码：

```cpp
#define CALL_WITH_MAX(a, b) f((a) > (b) ? (a) : (b))

void f(int num)
{
}
int main()
{
    int a = 5, b = 2;
    CALL_WITH_MAX(++a, b);         // a被累加两次
    cout << a << " " << b << "\n"; // 7 2
    CALL_WITH_MAX(++a, b + 10);    // a被累加一次
    cout << a << " " << b << "\n"; // 8 2
    return 0;
}
```

对于该宏`a`的递增次数会取决于“它被拿来和谁比较”。

事实上，可以使用内联函数来实现跟“宏定义函数”（实际不是函数）相同的效率，并且内联函数还会进行类型安全检查。对于上述代码，定义可以定义如下内联函数来避免使用宏定义所带来的问题

```cpp
template <typename T>
inline void call_with_max(T a, T b)
{
    f(a > b ? a : b);
}

int main()
{
    int a = 5, b = 2;
    call_with_max(++a, b);
    cout << a << " " << b << "\n"; // 6 2
    call_with_max(++a, b + 10);
    cout << a << " " << b << "\n"; // 7 2
    return 0;
}
```

此外，对于宏定义，还可以使用枚举类型`enum`来替代它。一方面`enum`的行为某些方面比较像`#define`，例如取一个`enum`的地址就不合法，而取一个`#define`的地址通常也不合法。

```cpp
#define NUM 10

enum{ MSIZE = 1000 };

int main()
{
    cout << &NUM << "\n";   // error: lvalue required as unary ‘&’ operand
    cout << &MSIZE << "\n"; // error: lvalue required as unary ‘&’ operand
    return 0;
}
```

**总结：**

- **对于单纯变量，最好以`const`对象或`enum`替换`#define`。**

- **对于形式函数的宏，最好改用`inline`函数。**



# 条款3：尽量使用`const`

`const`可以指定对象不可被改动，其可被施加于**任何作用域内的对象、函数参数、函数返回类型、成员函数本体**。

**`const`的限定规则**：如果关键字`const`出现在星号`*`左边，表示被指物常量，如果出现在星号右边，表示指针自身是常量，若出现在星号两边，则表示被指物和指针两者都是常量。

```cpp
int a = 10;
const int *b = &a;       // 非常量指针，常量数据
int *const c = &a;       // 常量指针，非常量数据
const int *const d = &a; // 常量指针，常量数据
```

> 更简便的限定规则记法：**若`const`左侧有东西，则其作用于左侧的东西，否则作用于右侧的东西**。

在C++中，若两个成员函数**如果只是常量性不同（一个是`const`成员函数，另一个不是），可以被重载**。

```cpp
class Text
{
private:
    string text;

public:
    Text(string text)
    {
        this->text = text;
    }
    const char &operator[](size_t pos) const
    {
        cout << "op1\n";
        return text[pos];
    }
    char &operator[](size_t pos)
    {
        cout << "op2\n";
        return text[pos];
    }
};

int main()
{
    Text t("hello world");
    cout << t[0] << "\n";
    const Text t1("hello world");
    cout << t1[1] << "\n";
    /*
        op2
        h
        op1
        e
    */
    return 0;
}
```

> 之所以能做到重载是因为类成员函数还会隐式传入一个`this`指针，而`const`成员函数会隐式传入一个`const this`指针。

注意：声明为`const`成员函数中可以修改被`mutable`修饰的成员变量。

**总结：**

- 将某些东西声明为`const`可帮助编译器侦测出错误用法。
- 编译器强制让`const`成员函数无法修改对象的任何内容，但实际上在编写程序时应该使用“概念上的强制性”。
- 当`const`和`non-const`成员函数有着实质等价的实现时，令`non-const`版本调用`const`版本可避免代码重复。



# 条款4：确定对象初始化

永远在使用对象之前先将它初始化。除了内置类型外，其他类型的初始化责任落在构造函数身上，规则很简单：**确保每个构造函数都将对象的每一个成员初始化**。

C++规定对象的**成员变量的初始化动作发生在进入构造函数本体之前**。`ABEntry`类的构造函数中`name`、`address`和`phones`都不是被初始化，而是赋值。这些对象的初始化发生于这些成员的`defult`构造函数被自动调用之时（比进入`ABEntry`构造函数本体的时间要早）。

```cpp
class PhoneNumber
{
private:
    string phone_number;

public:
    PhoneNumber(int phone_number)
    {
        this->phone_number = phone_number;
    }
};

class ABEntry
{
private:
    string name;
    string address;
    list<PhoneNumber> phones;
    int num_times_consulted;

public:
    ABEntry(const string &name, const string &address, const list<PhoneNumber> &phones)
    {
        this->name = name;
        this->address = address;
        this->phones = phones;
        num_times_consulted = 0;
    }
};

```

为此最佳的做法是采用**成员列表初始化**，即：

```cpp
ABEntry(const string &name, const string &address, const list<PhoneNumber> &phones) 
:name(name), address(address), phones(phones), num_times_consulted(0)
{
}
```

> 基于赋值的版本首先会调用`default`构造函数为对象成员变量设初值，然后立刻再对它们赋予新值。而采用成员列表初始化通常效率更高，因为初值列中针对各个成员变量设的实参，被拿去作为各成员变量拷贝构造函数的实参，这样要比先调用`default`构造函数再调用`copy assignment`操作符要更高效。

**总结：**

- 为内置对象进行手工初始化，因为C++不保证初始化它们。
- 构造函数最好使用成员列表初始化。