#ifndef CONNECTPOOL_H
#define CONNECTPOOL_H

#include <mysql/mysql.h>
#include <string>
#include <list>

#include "../log/log.h"

using std::string, std::list;

class connectPool
{
public:
    MYSQL *get_connection();              // 获取数据库连接
    bool release_connection(MYSQL *conn); // 释放连接
    int get_freeconn();                   // 获取连接
    void destroy_pool();                  // 销毁所有连接

    // 单例模式
    static connectPool *get_instance();

    void init(string url, string user, string password, string dbname,
              int port, int max_conn, int close_log);

private:
    connectPool();
    ~connectPool();

    connectPool(const connectPool &other) = delete;
    connectPool *operator=(const connectPool &other) = delete;

    int m_maxconn;  // 最大连接数
    int m_curconn;  // 当前已使用的连接数
    int m_freeconn; // 当前空闲的连接数
    locker m_lock;
    list<MYSQL *> m_connlist; // 连接池
    sem reserve;

public:
    string m_url;      // 主机地址
    string m_port;     // 数据库端口号
    string m_user;     // 登陆数据库用户名
    string m_password; // 登陆数据库密码
    string m_dbname;   // 使用数据库名
    int m_close_log;   // 日志开关
};

/**
 * 自动获取连接和释放连接RAII封装类
 */
class connectionRAII
{
public:
    connectionRAII(MYSQL **con, connectPool *connPool);
    ~connectionRAII();

private:
    MYSQL *conRAII;
    connectPool *poolRAII;
};
#endif