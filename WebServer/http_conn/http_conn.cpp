#include "http_conn.h"

// http响应的一些状态信息
const char *ok_200_title = "OK";
const char *error_400_title = "Bad Request";
const char *error_400_form = "Your request has bad syntax or is inherently impossible to staisfy.\n";
const char *error_403_title = "Forbidden";
const char *error_403_form = "You do not have permission to get file form this server.\n";
const char *error_404_title = "Not Found";
const char *error_404_form = "The requested file was not found on this server.\n";
const char *error_500_title = "Internal Error";
const char *error_500_form = "There was an unusual problem serving the request file.\n";

// 所有的客户数
int httpConn::m_user_count = 0;
// 所有socket上的事件都被注册到同一个epoll内核事件中，所以设置成静态的
int httpConn::m_epollfd = -1;
string httpConn::cur_user_name = "";
unordered_map<string, string> httpConn::m_users = {};

void httpConn::init_mysql_result(connectPool *conn_pool)
{
    // 从连接池中取一个连接
    MYSQL *mysql = NULL;
    connectionRAII mysqlcon(&mysql, conn_pool);

    // 在user表中检索username，passwd数据，浏览器端输入
    if (mysql_query(mysql, "SELECT username,password FROM user"))
    {
        LOG_ERROR("SELECT error:%s\n", mysql_error(mysql));
    }

    // 从表中检索完整的结果集
    MYSQL_RES *result = mysql_store_result(mysql);

    // 返回结果集中的列数
    int num_fields = mysql_num_fields(result);

    // 返回所有字段结构的数组
    MYSQL_FIELD *fields = mysql_fetch_fields(result);

    // 从结果集中获取下一行，将对应的用户名和密码，存入map中
    while (MYSQL_ROW row = mysql_fetch_row(result))
    {
        string temp1(row[0]);
        string temp2(row[1]);
        m_users[temp1] = temp2;
    }
}

/**
 * 对文件描述符设置非阻塞
 */
int setnonblocking(int fd)
{
    int old_option = fcntl(fd, F_GETFL);
    int new_option = old_option | O_NONBLOCK;
    fcntl(fd, F_SETFL, new_option);
    return old_option;
}

/**
 * 向epoll中添加需要监听的文件描述符
 * trig_mode: 1表示边缘触发 0表示水平触发
 */
void addfd(int epollfd, int fd, bool one_shot, int trig_mode)
{
    epoll_event event;
    event.data.fd = fd;

    if (trig_mode == 1)
        event.events = EPOLLIN | EPOLLET | EPOLLRDHUP;
    else
        event.events = EPOLLIN | EPOLLRDHUP;
    // 防止同一个通信被不同的线程处理
    if (one_shot)
        event.events |= EPOLLONESHOT;
    epoll_ctl(epollfd, EPOLL_CTL_ADD, fd, &event);
    setnonblocking(fd);
}

/**
 * 从epoll中移除监听的文件描述符
 */
void removefd(int epollfd, int fd)
{
    epoll_ctl(epollfd, EPOLL_CTL_DEL, fd, 0);
    close(fd);
}

/**
 * 修改文件描述符，重置socket上的EPOLLONESHOT事件，以确保下一次可读时，EPOLLIN事件能被触发
 */
void modfd(int epollfd, int fd, int ev, int trig_mode)
{
    epoll_event event;
    event.data.fd = fd;

    if (trig_mode == 1)
        event.events = ev | EPOLLET | EPOLLONESHOT | EPOLLRDHUP;
    else
        event.events = ev | EPOLLONESHOT | EPOLLRDHUP;

    epoll_ctl(epollfd, EPOLL_CTL_MOD, fd, &event);
}

/**
 * 关闭连接，关闭一个连接，客户总量减一
 */
void httpConn::close_conn(bool real_close)
{
    if (real_close && (m_sockfd != -1))
    {
        printf("close %d\n", m_sockfd);
        removefd(m_epollfd, m_sockfd);
        m_sockfd = -1;
        m_user_count--;
    }
}

/**
 * 初始化连接,外部调用初始化套接字地址
 */
void httpConn::init(int sockfd, const sockaddr_in &addr, char *root, int trig_mode,
                    int close_log, string user, string passwd, string sqlname)
{
    m_sockfd = sockfd;
    m_address = addr;

    addfd(m_epollfd, sockfd, true, m_trig_mode);
    m_user_count++;

    // 当浏览器出现连接重置时，可能是网站根目录出错或http响应格式出错或者访问的文件中内容完全为空
    doc_root = root;
    m_trig_mode = trig_mode;
    m_close_log = close_log;

    strcpy(sql_user, user.c_str());
    strcpy(sql_passwd, passwd.c_str());
    strcpy(sql_name, sqlname.c_str());

    init();
}

/**
 * 初始化新接受的连接
 * check_state默认为分析请求行状态
 */
void httpConn::init()
{
    mysql = NULL;
    bytes_to_send = 0;
    bytes_have_send = 0;
    m_check_state = CHECK_STATE_REQUESTLINE; // 初始状态为检查请求行
    m_linger = false;                        // 默认不保持链接  Connection : keep-alive保持连接
    m_method = GET;
    m_url = 0;
    m_version = 0;
    m_content_length = 0;
    m_host = 0;
    m_start_line = 0;
    m_checked_idx = 0;
    m_read_idx = 0;
    m_write_idx = 0;
    cgi = 0;
    m_state = 0;
    timer_flag = 0;
    improv = 0;

    memset(m_read_buf, '\0', READ_BUFFER_SIZE);
    memset(m_write_buf, '\0', WRITE_BUFFER_SIZE);
    memset(m_real_file, '\0', FILENAME_LEN);
}

/**
 * 从状态机，用于分析出一行内容
 * 返回值为行的读取状态，有LINE_OK,LINE_BAD,LINE_OPEN
 * 每行都已\r\n结尾
 */
httpConn::LINE_STATUS httpConn::parse_line()
{
    char temp;
    for (; m_checked_idx < m_read_idx; ++m_checked_idx)
    {
        temp = m_read_buf[m_checked_idx];
        if (temp == '\r')
        {
            if ((m_checked_idx + 1) == m_read_idx) // 当前尚未读取完整的行数据
                return LINE_OPEN;
            else if (m_read_buf[m_checked_idx + 1] == '\n') // 读取到完整的一行
            {
                m_read_buf[m_checked_idx++] = '\0';
                m_read_buf[m_checked_idx++] = '\0';
                return LINE_OK;
            }
            return LINE_BAD;
        }
        else if (temp == '\n')
        {
            // 若当前字符的前一个字符是\r
            if (m_checked_idx > 1 && m_read_buf[m_checked_idx - 1] == '\r')
            {
                m_read_buf[m_checked_idx - 1] = '\0';
                m_read_buf[m_checked_idx++] = '\0';
                return LINE_OK;
            }
            return LINE_BAD;
        }
    }
    return LINE_OPEN;
}

/**
 * 循环读取客户数据，直到无数据可读或对方关闭连接
 * 非阻塞ET工作模式下，需要一次性将数据读完
 */
bool httpConn::read_once()
{
    if (m_read_idx >= READ_BUFFER_SIZE)
    {
        return false;
    }
    // 读取的字节数
    int bytes_read = 0;

    // LT读取数据
    if (m_trig_mode == 0)
    {
        // 从m_read_buf + m_read_idx索引出开始保存数据，大小是READ_BUFFER_SIZE - m_read_idx
        bytes_read = recv(m_sockfd, m_read_buf + m_read_idx, READ_BUFFER_SIZE - m_read_idx, 0);
        m_read_idx += bytes_read;

        if (bytes_read <= 0)
        {
            return false;
        }

        return true;
    }
    // ET读数据
    else
    {
        while (true)
        {
            bytes_read = recv(m_sockfd, m_read_buf + m_read_idx, READ_BUFFER_SIZE - m_read_idx, 0);
            if (bytes_read == -1)
            {
                // 没有数据
                if (errno == EAGAIN || errno == EWOULDBLOCK)
                    break;
                return false;
            }
            else if (bytes_read == 0) // 对方关闭连接
            {
                return false;
            }
            m_read_idx += bytes_read;
        }
        return true;
    }
}

/**
 * 解析http请求行，获得请求方法，目标url及http版本号
 * text: // GET /index.html HTTP/1.1
 */
httpConn::HTTP_CODE httpConn::parse_request_line(char *text)
{
    // GET /index.html HTTP/1.1
    m_url = strpbrk(text, " \t"); // 判断第二个参数中的字符哪个在text中最先出现
    if (!m_url)
    {
        return BAD_REQUEST;
    }
    *m_url++ = '\0'; // 置位空字符，字符串结束符
    char *method = text;
    if (strcasecmp(method, "GET") == 0) // 忽略大小写比较
        m_method = GET;
    else if (strcasecmp(method, "POST") == 0)
    {
        m_method = POST;
        cgi = 1;
    }
    else
        return BAD_REQUEST;
    // /index.html HTTP/1.1
    m_url += strspn(m_url, " \t");
    m_version = strpbrk(m_url, " \t"); // 获取HTTP版本
    if (!m_version)
        return BAD_REQUEST;
    *m_version++ = '\0';
    m_version += strspn(m_version, " \t");
    if (strcasecmp(m_version, "HTTP/1.1") != 0)
        return BAD_REQUEST;
    //  http://127.0.0.1:80/index.html
    if (strncasecmp(m_url, "http://", 7) == 0)
    {
        m_url += 7;                 // 127.0.0.1:80/index.html
        m_url = strchr(m_url, '/'); // index.html
    }
    //  https://127.0.0.1:80/index.html
    if (strncasecmp(m_url, "https://", 8) == 0)
    {
        m_url += 8;
        m_url = strchr(m_url, '/');
    }

    if (!m_url || m_url[0] != '/')
        return BAD_REQUEST;
    // 当url为/时，显示判断界面
    if (strlen(m_url) == 1)
        strcat(m_url, "judge.html");
    m_check_state = CHECK_STATE_HEADER; // 检查状态变成检查头
    return NO_REQUEST;
}

/**
 * 解析http请求的一个头部信息
 */
httpConn::HTTP_CODE httpConn::parse_headers(char *text)
{
    // 遇到空行，表示头部字段解析完毕
    if (text[0] == '\0')
    {
        // 如果HTTP请求有消息体，则还需要读取m_content_length字节的消息体
        // 状态机转移到CHECK_STATE_CONTENT状态
        if (m_content_length != 0)
        {
            m_check_state = CHECK_STATE_CONTENT;
            return NO_REQUEST;
        }
        // 否则说明我们已经得到了一个完整的HTTP请求
        return GET_REQUEST;
    }
    else if (strncasecmp(text, "Connection:", 11) == 0)
    {
        // 处理Connection 头部字段  Connection: keep-alive
        text += 11;
        text += strspn(text, " \t");
        if (strcasecmp(text, "keep-alive") == 0)
        {
            m_linger = true;
        }
    }
    else if (strncasecmp(text, "Content-length:", 15) == 0)
    {
        // 处理Content-Length头部字段 Content-Length: 0
        text += 15;
        text += strspn(text, " \t");
        m_content_length = atol(text);
    }
    else if (strncasecmp(text, "Host:", 5) == 0)
    {
        // 处理Host头部字段 Host: 172.31.18.178:9006
        text += 5;
        text += strspn(text, " \t");
        m_host = text;
    }
    else
    {
        LOG_INFO("oop!unknow header: %s", text);
    }
    return NO_REQUEST;
}

/**
 * 判断http请求的请求体是否被完整读入
 */
httpConn::HTTP_CODE httpConn::parse_content(char *text)
{
    if (m_read_idx >= (m_content_length + m_checked_idx))
    {
        text[m_content_length] = '\0';
        // POST请求中最后为输入的用户名和密码
        m_string = text;
        return GET_REQUEST;
    }
    return NO_REQUEST;
}

/**
 * 主状态机，解析请求
 */
httpConn::HTTP_CODE httpConn::process_read()
{
    LINE_STATUS line_status = LINE_OK;
    HTTP_CODE ret = NO_REQUEST;
    char *text = 0;
    // 在满足一定条件下，不断解析行，并执行相应的操作，直到某些条件不再满足为止
    while ((m_check_state == CHECK_STATE_CONTENT && line_status == LINE_OK) ||
           ((line_status = parse_line()) == LINE_OK))
    {
        // 获取一行数据
        text = get_line();
        m_start_line = m_checked_idx;
        LOG_INFO("%s", text);
        switch (m_check_state)
        {
        // 解析请求行: 从请求中解析出请求方法、URL和协议版本
        case CHECK_STATE_REQUESTLINE:
        {
            ret = parse_request_line(text);
            if (ret == BAD_REQUEST)
                return BAD_REQUEST;
            break;
        }
        // 解析请求头：解析请求中的各个头部字段，如Content-Type
        case CHECK_STATE_HEADER:
        {
            ret = parse_headers(text);
            if (ret == BAD_REQUEST)
                return BAD_REQUEST;
            else if (ret == GET_REQUEST)
            {
                return do_request();
            }
            break;
        }
        // 解析请求体：如果请求包含请求体（例如POST请求），则解析请求体的内容
        case CHECK_STATE_CONTENT:
        {
            ret = parse_content(text);
            if (ret == GET_REQUEST)
                return do_request();
            line_status = LINE_OPEN;
            break;
        }
        default:
            return INTERNAL_ERROR;
        }
    }
    return NO_REQUEST;
}

/**
 * 当得到一个完整、正确的HTTP请求时，我们就分析目标文件的属性，
 * 如果目标文件存在、对所有用户可读，且不是目录，则使用mmap将其
 * 映射到内存地址m_file_address处，并告诉调用者获取文件成功
 */
httpConn::HTTP_CODE httpConn::do_request()
{
    strcpy(m_real_file, doc_root);
    int len = strlen(doc_root);
    // printf("m_url:%s\n", m_url);
    const char *p = strrchr(m_url, '/');

    // 处理cgi
    if (cgi == 1 && (*(p + 1) == '2' || *(p + 1) == '3' || *(p + 1) == '8'))
    {

        // 根据标志判断是登录检测还是注册检测还是修改
        char flag = m_url[1];

        char *m_url_real = new char[200];
        strcpy(m_url_real, "/");
        strcat(m_url_real, m_url + 2);
        strncpy(m_real_file + len, m_url_real, FILENAME_LEN - len - 1);
        delete m_url_real;

        // 将用户名和密码提取出来
        // user=123&passwd=123
        char name[100], password[100];
        int i;
        for (i = 5; m_string[i] != '&'; ++i)
            name[i - 5] = m_string[i];
        name[i - 5] = '\0';
        int j = 0;
        for (i = i + 10; m_string[i] != '\0'; ++i, ++j)
            password[j] = m_string[i];
        password[j] = '\0';
        if (*(p + 1) == '3')
        {
            // 如果是注册，先检测数据库中是否有重名的
            // 没有重名的，进行增加数据
            char *sql_insert = new char[200];
            strcpy(sql_insert, "INSERT INTO user(username, password) VALUES(");
            strcat(sql_insert, "'");
            strcat(sql_insert, name);
            strcat(sql_insert, "', '");
            strcat(sql_insert, password);
            strcat(sql_insert, "')");
            if (m_users.find(name) == m_users.end())
            {
                m_lock.lock();
                int res = mysql_query(mysql, sql_insert);
                m_users[name] = password;
                m_lock.unlock();

                if (!res)
                    strcpy(m_url, "/log.html");
                else
                    strcpy(m_url, "/registerError.html");
            }
            else
                strcpy(m_url, "/registerError.html");
            delete sql_insert;
        }
        // 如果是登录，直接判断
        // 若浏览器端输入的用户名和密码在表中可以查找到，返回1，否则返回0
        else if (*(p + 1) == '2')
        {
            if (m_users.count(name) && m_users[name] == password)
            {
                cur_user_name = name; // 登录的时候获取当前的用户名
                strcpy(m_url, "/welcome.html");
            }
            else
                strcpy(m_url, "/logError.html");
        }
        else if (*(p + 1) == '8')
        {
            char *sql_update = new char[200];
            strcpy(sql_update, "UPDATE user SET password = ");
            strcat(sql_update, "'");
            strcat(sql_update, password);
            strcat(sql_update, "'");
            strcat(sql_update, " WHERE username = ");
            strcat(sql_update, "'");
            strcat(sql_update, name);
            strcat(sql_update, "'");
            // 当前登录的用户只能更改当前的密码
            if (cur_user_name != name)
            {
                strcpy(m_url, "/changeForbidden.html");
            }
            else
            {
                m_lock.lock();
                int res = mysql_query(mysql, sql_update);
                m_users[name] = password;
                m_lock.unlock();

                if (!res)
                    strcpy(m_url, "/changeSuccess.html");
                else
                    strcpy(m_url, "/changeFailure.html");
            }
            delete[] sql_update;
        }
    }

    if (*(p + 1) == '0')
    {
        char *m_url_real = new char[200];
        strcpy(m_url_real, "/register.html");
        strncpy(m_real_file + len, m_url_real, strlen(m_url_real));

        delete m_url_real;
    }
    else if (*(p + 1) == '1')
    {
        char *m_url_real = new char[200];
        strcpy(m_url_real, "/log.html");
        strncpy(m_real_file + len, m_url_real, strlen(m_url_real));

        delete m_url_real;
    }
    else if (*(p + 1) == '5')
    {
        char *m_url_real = new char[200];
        strcpy(m_url_real, "/picture.html");
        strncpy(m_real_file + len, m_url_real, strlen(m_url_real));

        delete m_url_real;
    }
    else if (*(p + 1) == '6')
    {
        char *m_url_real = new char[200];
        strcpy(m_url_real, "/video.html");
        strncpy(m_real_file + len, m_url_real, strlen(m_url_real));

        delete m_url_real;
    }
    else if (*(p + 1) == '7')
    {
        char *m_url_real = new char[200];
        strcpy(m_url_real, "/change.html");
        strncpy(m_real_file + len, m_url_real, strlen(m_url_real));

        delete m_url_real;
    }
    else
        strncpy(m_real_file + len, m_url, FILENAME_LEN - len - 1);

    // 获取m_real_file文件的相关的状态信息，-1失败，0成功
    if (stat(m_real_file, &m_file_stat) < 0)
        return NO_RESOURCE;

    // 判断访问权限
    if (!(m_file_stat.st_mode & S_IROTH))
        return FORBIDDEN_REQUEST;

    // 判断是否是目录
    if (S_ISDIR(m_file_stat.st_mode))
        return BAD_REQUEST;

    // 以只读方式打开文件
    int fd = open(m_real_file, O_RDONLY);
    // 创建内存映射
    void *map_res = mmap(NULL, m_file_stat.st_size, PROT_READ, MAP_PRIVATE, fd, 0);
    assert(map_res != MAP_FAILED);
    m_file_address = (char *)map_res;
    // m_file_address = (char *)mmap(NULL, m_file_stat.st_size, PROT_READ, MAP_PRIVATE, fd, 0);
    close(fd);
    return FILE_REQUEST;
}

/**
 * 释放内存映射
 */
void httpConn::unmap()
{
    if (m_file_address)
    {
        munmap(m_file_address, m_file_stat.st_size);
        m_file_address = NULL;
    }
}

/**
 * 写HTTP响应
 */
bool httpConn::write()
{
    int temp = 0;

    if (bytes_to_send == 0)
    {
        // 将要发送的字节为0，这一次响应结束。
        modfd(m_epollfd, m_sockfd, EPOLLIN, m_trig_mode);
        init();
        return true;
    }

    while (1)
    {
        // 将数据从多个缓冲区一次性写入
        // writev函数成功时返回已写入的字节数，失败时返回-1
        temp = writev(m_sockfd, m_iv, m_iv_count);

        if (temp < 0)
        {
            // 如果TCP写缓冲没有空间，则等待下一轮EPOLLOUT事件，虽然在此期间
            // 服务器无法立即接收到同一客户的下一个请求，但可以保证连接的完整性
            if (errno == EAGAIN)
            {
                modfd(m_epollfd, m_sockfd, EPOLLOUT, m_trig_mode);
                return true;
            }
            unmap();
            return false;
        }

        // ???
        bytes_have_send += temp;
        bytes_to_send -= temp;
        if (bytes_have_send >= m_iv[0].iov_len)
        {
            m_iv[0].iov_len = 0;
            m_iv[1].iov_base = m_file_address + (bytes_have_send - m_write_idx);
            m_iv[1].iov_len = bytes_to_send;
        }
        else
        {
            m_iv[0].iov_base = m_write_buf + bytes_have_send;
            m_iv[0].iov_len = m_iv[0].iov_len - bytes_have_send;
        }

        if (bytes_to_send <= 0)
        {
            // 没有数据要发送
            unmap();
            modfd(m_epollfd, m_sockfd, EPOLLIN, m_trig_mode);

            if (m_linger)
            {
                init();
                return true;
            }
            else
            {
                return false;
            }
        }
    }
}

/**
 * 往写缓冲中写入待发送的数据
 */
bool httpConn::add_response(const char *format, ...)
{
    if (m_write_idx >= WRITE_BUFFER_SIZE)
        return false;
    va_list arg_list;
    va_start(arg_list, format);
    int len = vsnprintf(m_write_buf + m_write_idx, WRITE_BUFFER_SIZE - 1 - m_write_idx, format, arg_list);
    if (len >= (WRITE_BUFFER_SIZE - 1 - m_write_idx))
    {
        va_end(arg_list);
        return false;
    }
    m_write_idx += len;
    va_end(arg_list);

    LOG_INFO("request:%s", m_write_buf);

    return true;
}

/**
 * 添加状态行
 */
bool httpConn::add_status_line(int status, const char *title)
{
    return add_response("%s %d %s\r\n", "HTTP/1.1", status, title);
}

/**
 * 添加头部
 */
bool httpConn::add_headers(int content_len)
{
    return add_content_length(content_len) && add_linger() && add_blank_line();
}

/**
 * 添加内容长度字段
 */
bool httpConn::add_content_length(int content_len)
{
    return add_response("Content-Length:%d\r\n", content_len);
}

/**
 * 添加内容类型
 */
bool httpConn::add_content_type()
{
    return add_response("Content-Type:%s\r\n", "text/html");
}

/**
 * 添加是否开启长连接
 */
bool httpConn::add_linger()
{
    return add_response("Connection:%s\r\n", (m_linger == true) ? "keep-alive" : "close");
}

/**
 * 添加空行（分隔请求头和请求体）
 */
bool httpConn::add_blank_line()
{
    return add_response("%s", "\r\n");
}

/**
 * 添加请求内容
 */
bool httpConn::add_content(const char *content)
{
    return add_response("%s", content);
}

/**
 * 根据服务器处理HTTP请求的结果，决定返回给客户端的内容
 */
bool httpConn::process_write(HTTP_CODE ret)
{
    switch (ret)
    {
    case INTERNAL_ERROR:
    {
        add_status_line(500, error_500_title);
        add_headers(strlen(error_500_form));
        if (!add_content(error_500_form))
            return false;
        break;
    }
    case BAD_REQUEST:
    {
        add_status_line(404, error_404_title);
        add_headers(strlen(error_404_form));
        if (!add_content(error_404_form))
            return false;
        break;
    }
    case FORBIDDEN_REQUEST:
    {
        add_status_line(403, error_403_title);
        add_headers(strlen(error_403_form));
        if (!add_content(error_403_form))
            return false;
        break;
    }
    case FILE_REQUEST:
    {
        add_status_line(200, ok_200_title);
        if (m_file_stat.st_size != 0)
        {
            add_headers(m_file_stat.st_size);
            m_iv[0].iov_base = m_write_buf;
            m_iv[0].iov_len = m_write_idx;
            m_iv[1].iov_base = m_file_address;
            m_iv[1].iov_len = m_file_stat.st_size;
            m_iv_count = 2;
            bytes_to_send = m_write_idx + m_file_stat.st_size;
            return true;
        }
        else
        {
            const char *ok_string = "<html><body></body></html>";
            add_headers(strlen(ok_string));
            if (!add_content(ok_string))
                return false;
        }
    }
    default:
        return false;
    }
    m_iv[0].iov_base = m_write_buf;
    m_iv[0].iov_len = m_write_idx;
    m_iv_count = 1;
    bytes_to_send = m_write_idx;
    return true;
}

/**
 * 由线程池中的工作线程调用，处理HTTP请求的入口函数
 */
void httpConn::process()
{
    HTTP_CODE read_ret = process_read();
    if (read_ret == NO_REQUEST)
    {
        modfd(m_epollfd, m_sockfd, EPOLLIN, m_trig_mode);
        return;
    }
    bool write_ret = process_write(read_ret);
    if (!write_ret)
    {
        close_conn();
    }
    modfd(m_epollfd, m_sockfd, EPOLLOUT, m_trig_mode);
}
