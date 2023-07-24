#include "config.h"

int main(int argc, char *argv[])
{
    // 需要修改的数据库信息,登录名,密码,库名
    string user = "your_user_name";
    string passwd = "your_password";
    string databasename = "your_db_name";

    // 命令行解析
    Config config;
    config.parse_arg(argc, argv);

    WebServer server;

    // 初始化
    server.init(config.port, user, passwd, databasename, config.log_write,
                config.opt_linger, config.trig_mode, config.sql_num, config.thread_num,
                config.close_log, config.actor_model);

    // 日志
    server.set_log_write();

    // 数据库
    server.set_db_pool();

    // 线程池
    server.set_thread_pool();

    // 触发模式
    server.set_trig_mode();

    // 监听
    server.set_event_listen();

    // 运行
    server.set_event_loop();

    return 0;
}