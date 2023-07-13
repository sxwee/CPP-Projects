#ifndef WEBSERVER_H
#define WEBSERVER_H

#include <string>
#include <assert.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <stdio.h>
#include <unistd.h>
#include <errno.h>
#include <fcntl.h>
#include <stdlib.h>
#include <cassert>
#include <sys/epoll.h>

#include "./http_conn/http_conn.h"
#include "./db/connect_pool.h"
#include "./thread_pool/thread_pool.h"
#include "./timer/lst_timer.h"
#include "./log/log.h"

using std::string;

const int MAX_FD = 65536;          // 最大文件描述符数
const int MAX_EVENT_NUMBER = 1000; // 监听的最大的事件数量
const int TIMESLOT = 5;            // 最小超时单位
const int ROOT_SIZE = 256;         // 网站根目录

class WebServer
{
public:
    WebServer();
    ~WebServer();

    void init(int port, string user, string pwd, string db_name, int log_write,
              int opt_linger, int trig_mode, int sql_num, int thread_num,
              int close_log, int actor_model);

    void set_thread_pool();
    void set_db_pool();
    void set_log_write();
    void set_trig_mode();
    void set_event_listen();
    void set_event_loop();
    void timer(int connfd, struct sockaddr_in client_address);
    void adjust_timer(util_timer *timer);
    void deal_timer(util_timer *timer, int sockfd);
    bool deal_client_data();
    bool deal_with_signal(bool &timeout, bool &stop_server);
    void deal_with_read(int sockfd);
    void deal_with_write(int sockfd);

private:
    // 网络连接相关
    int m_port;             // 端口号
    char m_root[ROOT_SIZE]; // 网站根目录
    int m_log_write;        // 日志写入方式
    int m_close_log;        // 是否关闭日志
    int m_actor_mode;       // 并发模型选择 0: proactor 1: reactor
    int m_pipefd[2];        // ???
    int m_epollfd;          // epoll监听文件描述符
    httpConn *users;        // HTTP连接数

    // 数据库相关
    connectPool *m_conn_pool; // 数据库连接池
    string m_user;            // 数据库用户名
    string m_pwd;             // 数据库登陆密码
    string m_db_name;         // 数据库名
    int m_sql_num;            // 数据库连接数

    // 线程池相关
    threadPool<httpConn> *m_event_pool; // 事件处理线程池
    int m_thread_num;             // 线程池线程数

    // Epoll Event相关
    epoll_event events[MAX_EVENT_NUMBER];

    int m_listenfd;         // 服务器的监听文件描述符
    int m_opt_linger;       // 是否优雅关闭链接
    int m_trig_mode;        // 触发模式 0: LT 1: ET
    int m_listen_trig_mode; // 服务器监听套接字的触发方式 0: LT 1: ET
    int m_conn_trig_mode;   // 连接套接字的触发方式 0: LT 1: ET

    // 定时器相关
    client_data *users_timer;
    Utils utils;
};

#endif