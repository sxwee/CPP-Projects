#ifndef HTTPCONN_H
#define HTTPCONN_H

#include <unistd.h>
#include <signal.h>
#include <sys/types.h>
#include <sys/epoll.h>
#include <fcntl.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <assert.h>
#include <sys/stat.h>
#include <string.h>
#include <pthread.h>
#include <stdio.h>
#include <stdlib.h>
#include <sys/mman.h>
#include <stdarg.h>
#include <errno.h>
#include <sys/wait.h>
#include <sys/uio.h>
#include <unordered_map>
#include <fstream>

#include "../db/connect_pool.h"
#include "../log/log.h"
#include "../timer/lst_timer.h"
#include "../lock/locker.h"

using std::pair;
using std::string;
using std::unordered_map;

class httpConn
{
public:
    static const int FILENAME_LEN = 200;       // 文件名的最大长度
    static const int READ_BUFFER_SIZE = 2048;  // 读缓冲区的大小
    static const int WRITE_BUFFER_SIZE = 1024; // 写缓冲区的大小
    // HTTP请求方法, 本项目只实现了GET和POST请求
    enum METHOD
    {
        GET = 0,
        POST,
        HEAD,
        PUT,
        DELETE,
        TRACE,
        OPTIONS,
        CONNECT,
        PATH
    };
    /**
     * 解析客户端请求时，主状态机的状态
     * CHECK_STATE_REQUESTLINE: 当前正在分析请求行
     * CHECK_STATE_HEADER: 当前正在分析头部字段
     * CHECK_STATE_CONTENT: 当前正在解析请求体
     */
    enum CHECK_STATE
    {
        CHECK_STATE_REQUESTLINE = 0,
        CHECK_STATE_HEADER,
        CHECK_STATE_CONTENT
    };
    /**
     *
     * 服务器处理HTTP请求的可能结果，报文解析的结果
     * NO_REQUEST          :   请求不完整，需要继续读取客户数据
     * GET_REQUEST         :   表示获得了一个完成的客户请求
     * BAD_REQUEST         :   表示客户请求语法错误
     * NO_RESOURCE         :   表示服务器没有资源
     * FORBIDDEN_REQUEST   :   表示客户对资源没有足够的访问权限
     * FILE_REQUEST        :   文件请求,获取文件成功
     * INTERNAL_ERROR      :   表示服务器内部错误
     * CLOSED_CONNECTION   :   表示客户端已经关闭连接了
     */
    enum HTTP_CODE
    {
        NO_REQUEST,
        GET_REQUEST,
        BAD_REQUEST,
        NO_RESOURCE,
        FORBIDDEN_REQUEST,
        FILE_REQUEST,
        INTERNAL_ERROR,
        CLOSED_CONNECTION
    };
    /**
     * 从状态机的三种可能状态，即行的读取状态，分别表示
     * LINE_OK: 读取到一个完整的行
     * LINE_BAD: 行出错
     * LINE_OPEN: 行数据尚且不完整
     */
    enum LINE_STATUS
    {
        LINE_OK = 0,
        LINE_BAD,
        LINE_OPEN
    };

public:
    httpConn() {}
    ~httpConn() {}

public:
    void init(int sockfd, const sockaddr_in &addr, char *root, int trig_mode, int close_log,
              string user, string passwd, string sqlname);
    void close_conn(bool real_close = true);
    void process();
    bool read_once();
    bool write();
    inline sockaddr_in *get_address()
    {
        return &m_address;
    }
    void init_mysql_result(connectPool *conn_pool);
    int timer_flag;
    int improv;

private:
    void init();                       // 初始化连接
    HTTP_CODE process_read();          // 解析HTTP请求
    bool process_write(HTTP_CODE ret); // 填充HTTP应答

    // 被process_read调用以分析HTTP请求的函数组
    HTTP_CODE parse_request_line(char *text);
    HTTP_CODE parse_headers(char *text);
    HTTP_CODE parse_content(char *text);
    HTTP_CODE do_request();
    char *get_line() { return m_read_buf + m_start_line; };
    LINE_STATUS parse_line();

    // 被process_write调用以完成HTTP应答的函数组
    void unmap();
    bool add_response(const char *format, ...);
    bool add_content(const char *content);
    bool add_status_line(int status, const char *title);
    bool add_headers(int content_length);
    bool add_content_type();
    bool add_content_length(int content_length);
    bool add_linger();
    bool add_blank_line();

public:
    static int m_epollfd;                         // 所有socket上的事件都被注册到同一个epoll内核事件中，全局共享
    static int m_user_count;                      // 统计用户的数量
    static unordered_map<string, string> m_users; // <用户名, 密码>
    static string cur_user_name;                  // 记录当前登录的用户名
    MYSQL *mysql;                                 // MySQL连接
    int m_state;                                  // 读为0, 写为1

private:
    int m_sockfd; // 连接对应的套接字
    sockaddr_in m_address;
    char m_read_buf[READ_BUFFER_SIZE]; // 读缓冲区
    int m_read_idx;                    // 标识读缓冲区中已经读入的客户端数据的最后一个字节的下一个位置
    int m_checked_idx;                 // 当前正在分析的字符在读缓冲区中的位置
    int m_start_line;                  // 当前正在解析的行的起始位置

    char m_write_buf[WRITE_BUFFER_SIZE]; // 写缓冲区
    int m_write_idx;                     // 指示写缓冲区的长度

    CHECK_STATE m_check_state; // 主状态机当前所处的状态
    METHOD m_method;           // 请求方法

    char m_real_file[FILENAME_LEN]; // 客户请求的目标文件的完整路径，其内容等于 doc_root + m_url, doc_root是网站根目录
    char *m_url;                    // 客户请求的目标文件的文件名
    char *m_version;                // HTTP协议版本号
    char *m_host;                   // 主机名
    int m_content_length;           // HTTP请求的消息总长度
    bool m_linger;                  // HTTP请求是否要求保持连接

    char *m_file_address;    // 客户请求的目标文件被mmap到内存中的起始位置
    struct stat m_file_stat; // 目标文件的状态。通过它可以判断文件是否存在、是否为目录、是否可读，并获取文件大小等信息
    struct iovec m_iv[2];    // 采用writev来执行写操作
    int m_iv_count;          // 被写内存块的数量

    int cgi;             // 是否启用POST
    char *m_string;      // 存储请求头数据
    int bytes_to_send;   // 将要发送的数据的字节数
    int bytes_have_send; // 已经发送的字节数

    char *doc_root; // 网站根目录

    locker m_lock;
    int m_trig_mode;
    int m_close_log;

    char sql_user[100];   // 数据库用户名
    char sql_passwd[100]; // 数据库登录密码
    char sql_name[100];   // 数据库名
};

#endif