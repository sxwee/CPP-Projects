#include "webserver.h"

WebServer::WebServer()
{
    users = new httpConn[MAX_FD];
    // 设置网络根目录
    strcpy(m_root, "./static");
    // 定时器
    users_timer = new client_data[MAX_FD];
}

WebServer::~WebServer()
{
    close(m_epollfd);
    close(m_listenfd);
    close(m_pipefd[1]);
    close(m_pipefd[0]);
    delete[] users;
    delete[] users_timer;
    delete m_event_pool;
}

void WebServer::init(int port, string user, string pwd, string db_name, int log_write,
                     int opt_linger, int trig_mode, int sql_num, int thread_num,
                     int close_log, int actor_model)
{
    m_port = port;
    m_user = user;
    m_pwd = pwd;
    m_db_name = db_name;
    m_log_write = log_write;
    m_opt_linger = opt_linger;
    m_trig_mode = trig_mode;
    m_sql_num = sql_num;
    m_thread_num = thread_num;
    m_close_log = close_log;
    m_actor_mode = actor_model;
}

/**
 * 初始化线程池
 */
void WebServer::set_thread_pool()
{
    m_event_pool = new threadPool<httpConn>(m_actor_mode, m_conn_pool, m_thread_num);
}

/**
 * 初始化数据库连接池
 */
void WebServer::set_db_pool()
{
    // 初始化数据库连接池
    m_conn_pool = connectPool::get_instance();
    m_conn_pool->init("127.0.0.1", m_user, m_pwd, m_db_name, 3306, m_sql_num, m_close_log);

    // 初始化数据库用户和密码
    users->init_mysql_result(m_conn_pool);
}

/**
 * 设置日志是否关闭
 * 0: 不关闭
 * 1: 关闭
 */
void WebServer::set_log_write()
{
    if (m_close_log == 0)
    {
        // 设置日志的写入方式
        // 1表示同步 0表示异步
        if (m_log_write == 1)
        {
            log::get_instance()->init("./log_dir/server_log", m_close_log, 2000, 800000, 800);
        }
        else
        {
            log::get_instance()->init("./log_dir/server_log", m_close_log, 2000, 800000, 0);
        }
    }
}

/**
 * 设置监听套接字和连接套接字的触发方式
 * 0: LT 水平触发
 * 1: ET 边缘触发
 */
void WebServer::set_trig_mode()
{
    switch (m_trig_mode)
    {
    // LT + LT
    case 0:
    {
        m_listen_trig_mode = 0;
        m_conn_trig_mode = 0;
        break;
    }
    // LT + ET
    case 1:
    {
        m_listen_trig_mode = 0;
        m_conn_trig_mode = 1;
        break;
    }
    // ET + LT
    case 2:
    {
        m_listen_trig_mode = 1;
        m_conn_trig_mode = 0;
        break;
    }
    // ET + ET
    case 3:
    {
        m_listen_trig_mode = 1;
        m_conn_trig_mode = 1;
        break;
    }
    default:
        break;
    }
}

/**
 * 设置服务器的监听套接字
 */
void WebServer::set_event_listen()
{
    m_listenfd = socket(AF_INET, SOCK_STREAM, 0);
    assert(m_listenfd >= 0);
    // 优雅关闭套接字
    if (m_opt_linger == 0)
    {
        // l_onoff为0表示套接字关闭采取默认行为
        // l_linger: 套接字在关闭前等待发送或接收的未完成数据的时间
        struct linger tmp = {0, 1};
        setsockopt(m_listenfd, SOL_SOCKET, SO_LINGER, &tmp, sizeof(tmp));
    }
    else if (m_opt_linger == 1)
    {
        // 套接字关闭后，将尝试发送或接收未完成的数据，最多等待l_linger秒
        struct linger tmp = {1, 1};
        setsockopt(m_listenfd, SOL_SOCKET, SO_LINGER, &tmp, sizeof(tmp));
    }

    // 初始化地址信息
    struct sockaddr_in address;
    bzero(&address, sizeof(address));
    address.sin_family = AF_INET;
    address.sin_addr.s_addr = htonl(INADDR_ANY);
    address.sin_port = htons(m_port);

    // 设置端口可复用
    int flag = 1;
    setsockopt(m_listenfd, SOL_SOCKET, SO_REUSEADDR, &flag, sizeof(flag));
    // 绑定端口
    int ret = bind(m_listenfd, (struct sockaddr *)&address, sizeof(address));
    assert(ret >= 0);
    // 开始监听
    ret = listen(m_listenfd, 5);
    assert(ret >= 0);

    // 初始化单位时间
    utils.init(TIMESLOT);

    // epoll创建内核事件表
    epoll_event events[MAX_EVENT_NUMBER];
    m_epollfd = epoll_create(5);
    assert(m_epollfd != -1);

    // 向epoll中添加需要监听的文件描述符
    utils.addfd(m_epollfd, m_listenfd, false, m_listen_trig_mode);
    httpConn::m_epollfd = m_epollfd;

    // 创建一对相互连接的套接字，多用于进程间通信
    // pipefd[0]用于读取数据
    // pipefd[1]用于写入数据
    ret = socketpair(PF_UNIX, SOCK_STREAM, 0, m_pipefd);
    assert(ret != -1);
    // 设置非阻塞
    utils.setnonblocking(m_pipefd[1]);
    // 向epoll中添加读取的管道套接字
    utils.addfd(m_epollfd, m_pipefd[0], false, 0);

    // 当一个进程向一个已关闭写端的管道（或者套接字）写入数据时，内核会向该进程发送 SIGPIPE 信号
    // SIG_IGN表示忽略信号
    utils.addsig(SIGPIPE, SIG_IGN);
    // SIGALRM信号通常用于实现定时功能
    utils.addsig(SIGALRM, utils.sig_handler, false);
    // SIGTERM通知进程应该正常终止并退出
    utils.addsig(SIGTERM, utils.sig_handler, false);

    // 设置每隔一段时间发送一个SIGALRM信号
    alarm(TIMESLOT);

    // 工具类,信号和描述符基础操作
    Utils::u_pipefd = m_pipefd;
    Utils::u_epollfd = m_epollfd;
}

void WebServer::set_event_loop()
{
    bool timeout = false;
    bool stop_server = false;

    while (!stop_server)
    {
        int number = epoll_wait(m_epollfd, events, MAX_EVENT_NUMBER, -1);
        if (number < 0 && errno != EINTR)
        {
            LOG_ERROR("%s", "epoll failure");
            break;
        }

        for (int i = 0; i < number; i++)
        {
            int sockfd = events[i].data.fd;

            // 处理新到的客户连接
            if (sockfd == m_listenfd)
            {
                bool flag = deal_client_data();
                if (false == flag)
                    continue;
            }
            // 服务器端关闭连接，移除对应的定时器
            // EPOLLRDHUP: 对端关闭连接或者关闭写入一半的情况
            // EPOLLHUP: 发生了挂起事件，通常是对方进程异常终止或者底层传输错误
            // EPOLLERR: 发生了错误事件，比如socket出错或者连接失败等
            else if (events[i].events & (EPOLLRDHUP | EPOLLHUP | EPOLLERR))
            {
                util_timer *timer = users_timer[sockfd].timer;
                deal_timer(timer, sockfd);
            }
            // 处理信号
            else if ((sockfd == m_pipefd[0]) && (events[i].events & EPOLLIN))
            {
                bool flag = deal_with_signal(timeout, stop_server);
                if (flag == false)
                    LOG_ERROR("%s", "dealclientdata failure");
            }
            // 处理客户连接上接收到的数据
            else if (events[i].events & EPOLLIN)
            {
                deal_with_read(sockfd);
            }
            else if (events[i].events & EPOLLOUT)
            {
                deal_with_write(sockfd);
            }
        }
        if (timeout)
        {
            utils.timer_handler();

            LOG_INFO("%s", "timer tick");

            timeout = false;
        }
    }
}

/**
 * 初始化client_data数据
 * 创建定时器，设置回调函数和超时时间，绑定用户数据，将定时器添加到链表中
 */
void WebServer::timer(int connfd, struct sockaddr_in client_address)
{
    users[connfd].init(connfd, client_address, m_root, m_conn_trig_mode,
                       m_close_log, m_user, m_pwd, m_db_name);
    users_timer[connfd].address = client_address;
    users_timer[connfd].sockfd = connfd;
    util_timer *timer = new util_timer;
    timer->user_data = &users_timer[connfd];
    timer->cb_func = cb_func;
    time_t cur = time(NULL);
    timer->expire = cur + 3 * TIMESLOT;
    users_timer[connfd].timer = timer;
    utils.m_timer_lst.add_timer(timer);
}

/**
 * 若有数据传输，则将定时器往后延迟3个单位
 * 并对新的定时器在链表上的位置进行调整
 */
void WebServer::adjust_timer(util_timer *timer)
{
    time_t cur = time(NULL);
    timer->expire = cur + 3 * TIMESLOT;
    utils.m_timer_lst.add_timer(timer);

    LOG_INFO("%s", "adjust timer once");
}

void WebServer::deal_timer(util_timer *timer, int sockfd)
{
    timer->cb_func(&users_timer[sockfd]);
    if (timer)
    {
        utils.m_timer_lst.del_timer(timer);
    }

    LOG_INFO("close fd %d", users_timer[sockfd].sockfd);
}

/**
 * 处理客户端的连接请求
 */
bool WebServer::deal_client_data()
{
    struct sockaddr_in client_address;
    socklen_t client_addrlength = sizeof(client_address);
    if (m_listen_trig_mode == 0)
    {
        int connfd = accept(m_listenfd, (struct sockaddr *)&client_address, &client_addrlength);
        if (connfd < 0)
        {
            LOG_ERROR("%s:errno is:%d", "accept error", errno);
            return false;
        }
        if (httpConn::m_user_count >= MAX_FD)
        {
            utils.show_error(connfd, "Internal server busy");
            LOG_ERROR("%s", "Internal server busy");
            return false;
        }
        timer(connfd, client_address);
    }
    // 边缘模式, 需要一次性处理完所有的连接请求
    else
    {
        while (true)
        {
            int connfd = accept(m_listenfd, (struct sockaddr *)&client_address, &client_addrlength);
            if (connfd < 0)
            {
                LOG_ERROR("%s:errno is:%d", "accept error", errno);
                break;
            }
            if (httpConn::m_user_count >= MAX_FD)
            {
                utils.show_error(connfd, "Internal server busy");
                LOG_ERROR("%s", "Internal server busy");
                break;
            }
            timer(connfd, client_address);
        }
        return false;
    }
    return true;
}

bool WebServer::deal_with_signal(bool &timeout, bool &stop_server)
{
    int ret = 0;
    int sig;
    char signals[1024];
    ret = recv(m_pipefd[0], signals, sizeof(signals), 0);
    if (ret == -1)
    {
        return false;
    }
    else if (ret == 0)
    {
        return false;
    }
    else
    {
        for (int i = 0; i < ret; ++i)
        {
            switch (signals[i])
            {
            case SIGALRM:
            {
                timeout = true;
                break;
            }
            case SIGTERM:
            {
                stop_server = true;
                break;
            }
            }
        }
    }
    return true;
}

void WebServer::deal_with_read(int sockfd)
{
    util_timer *timer = users_timer[sockfd].timer;

    // reactor
    if (m_actor_mode == 1)
    {
        if (timer)
        {
            adjust_timer(timer);
        }

        // 若监测到读事件，将该事件放入请求队列
        m_event_pool->append(users + sockfd, 0);

        while (true)
        {
            if (users[sockfd].improv == 1)
            {
                if (users[sockfd].timer_flag == 1)
                {
                    deal_timer(timer, sockfd);
                    users[sockfd].timer_flag = 0;
                }
                users[sockfd].improv = 0;
                break;
            }
        }
    }
    else
    {
        // proactor
        if (users[sockfd].read_once())
        {
            LOG_INFO("deal with the client(%s)", inet_ntoa(users[sockfd].get_address()->sin_addr));

            // 若监测到读事件，将该事件放入请求队列
            m_event_pool->append_p(users + sockfd);

            if (timer)
            {
                adjust_timer(timer);
            }
        }
        else
        {
            deal_timer(timer, sockfd);
        }
    }
}

void WebServer::deal_with_write(int sockfd)
{
    util_timer *timer = users_timer[sockfd].timer;
    // reactor
    if (m_actor_mode == 1)
    {
        if (timer)
        {
            adjust_timer(timer);
        }

        m_event_pool->append(users + sockfd, 1);

        while (true)
        {
            if (users[sockfd].improv == 1)
            {
                if (users[sockfd].timer_flag == 1)
                {
                    deal_timer(timer, sockfd);
                    users[sockfd].timer_flag = 0;
                }
                users[sockfd].improv = 0;
                break;
            }
        }
    }
    else
    {
        // proactor
        if (users[sockfd].write())
        {
            LOG_INFO("send data to the client(%s)", inet_ntoa(users[sockfd].get_address()->sin_addr));

            if (timer)
            {
                adjust_timer(timer);
            }
        }
        else
        {
            deal_timer(timer, sockfd);
        }
    }
}
