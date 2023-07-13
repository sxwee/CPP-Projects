#include "connect_pool.h"

connectPool::connectPool()
{
    m_curconn = 0;
    m_freeconn = 0;
}

connectPool *connectPool::get_instance()
{
    static connectPool conn_pool;
    return &conn_pool;
}

/**
 * 构造初始化, 创建若干个数据库连接
 */
void connectPool::init(string url, string user, string password, string dbname,
                       int port, int max_conn, int close_log)
{
    m_url = url;
    m_port = port;
    m_user = user;
    m_password = password;
    m_dbname = dbname;
    m_close_log = close_log;

    for (int i = 0; i < max_conn; i++)
    {
        // 初始化MySQL连接对象
        MYSQL *con = NULL;
        con = mysql_init(con);

        if (con == NULL)
        {
            LOG_ERROR("MySQL Error");
            exit(1);
        }
        // 设置数据库连接参数
        con = mysql_real_connect(con, url.c_str(), user.c_str(), password.c_str(),
                                 dbname.c_str(), port, NULL, 0);

        if (con == NULL)
        {
            LOG_ERROR("MySQL Error");
            mysql_close(con);
            exit(1);
        }
        m_connlist.emplace_back(con);
        // m_connlist.push_back(con);
        ++m_freeconn;
    }

    // 初始化数据库连接的信号量
    reserve = sem(m_freeconn);

    m_maxconn = m_freeconn;
}

/**
 * 当有请求时，从数据库连接池中返回一个可用连接，更新使用和空闲连接数
 */
MYSQL *connectPool::get_connection()
{
    MYSQL *con = NULL;

    if (m_connlist.size() == 0)
        return NULL;

    // 尝试对信号量加锁, 检查是否有空余的连接
    reserve.wait();

    m_lock.lock();

    con = m_connlist.front();
    m_connlist.pop_front();

    --m_freeconn;
    ++m_curconn;

    m_lock.unlock();
    return con;
}

/**
 * 释放当前使用的连接
 */
bool connectPool::release_connection(MYSQL *con)
{
    if (con == NULL)
        return false;

    m_lock.lock();

    // m_connlist.push_back(con);
    m_connlist.emplace_back(con);
    ++m_freeconn;
    --m_curconn;

    m_lock.unlock();

    reserve.post();
    return true;
}

/**
 * 销毁数据库连接池
 */
void connectPool::destroy_pool()
{

    m_lock.lock();
    if (m_connlist.size() > 0)
    {
        list<MYSQL *>::iterator it;
        for (it = m_connlist.begin(); it != m_connlist.end(); ++it)
        {
            MYSQL *con = *it;
            mysql_close(con);
        }
        m_curconn = 0;
        m_freeconn = 0;
        m_connlist.clear();
    }

    m_lock.unlock();
}

/**
 * 当前空闲的连接数
 */
int connectPool::get_freeconn()
{
    int ret = 0;
    m_lock.lock();
    ret = this->m_freeconn;
    m_lock.unlock();
    return ret;
}

connectPool::~connectPool()
{
    destroy_pool();
}

connectionRAII::connectionRAII(MYSQL **SQL, connectPool *connPool)
{
    *SQL = connPool->get_connection();

    conRAII = *SQL;
    poolRAII = connPool;
}

connectionRAII::~connectionRAII()
{
    poolRAII->release_connection(conRAII);
}