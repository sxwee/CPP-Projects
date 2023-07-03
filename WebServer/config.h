#ifndef CONFIG_H
#define CONFIG_H

class Config
{
public:
    int PORT;              // 服务端口号
    int LOG_WRITE;         // 日志写入方式 1: 异步写入 0: 同步写入
    int TRIG_MODE;         // 触发组合模式 LISTEN+CONNFD的模式组合 0: LT+LT 1: LT+ET 2: ET+LT 3:ET+ET
    int LISTEN_TRIG_MODE;  // 监听套接字触发模式
    int CONNECT_TRIG_MODE; // 连接套接字触发模式
    int OPT_LINGER;        // 优雅关闭连接 0: 不适用 1: 使用
    int DB_NUM;            // 数据库连接数量
    int THREAD_NUM;        // 线程池线程数
    int CLOSE_LOG;         // 是否使用日志 0: 打开日志 1: 关闭日志
    int ACTOR_MODEL;       // 并发模型选择 0: Proactor 1: Reactor

public:
    Config();
    ~Config(){};
    void set_config(int argc, char *argv[]);
};

#endif