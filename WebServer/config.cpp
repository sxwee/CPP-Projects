#include "config.h"
#include <unistd.h>
#include <stdlib.h>

Config::Config()
{
    PORT = 9547;           // 默认端口9547
    LOG_WRITE = 0;         // 默认同步
    TRIG_MODE = 0;         // 默认LISTEN LT + CONNECT LT
    LISTEN_TRIG_MODE = 0;  // 默认LT
    CONNECT_TRIG_MODE = 0; // 默认LT
    OPT_LINGER = 0;        // 默认不使用
    DB_NUM = 8;            // 默认连接数为8
    THREAD_NUM = 8;        // 默认线程池数为8
    CLOSE_LOG = 0;         // 默认使用日志
    ACTOR_MODEL = 0;       // 默认Proactor
}

void Config::set_config(int argc, char *argv[])
{
    int opt;
    const char *str = "p:l:m:o:d:t:c:a:";
    while ((opt = getopt(argc, argv, str)) != -1)
    {
        switch (opt)
        {
        case 'p':
        {
            PORT = atoi(optarg);
            break;
        }
        case 'l':
        {
            LOG_WRITE = atoi(optarg);
            break;
        }
        case 'm':
        {
            TRIG_MODE = atoi(optarg);
            break;
        }
        case 'o':
        {
            OPT_LINGER = atoi(optarg);
            break;
        }
        case 'd':
        {
            DB_NUM = atoi(optarg);
            break;
        }
        case 't':
        {
            THREAD_NUM = atoi(optarg);
            break;
        }
        case 'c':
        {
            CLOSE_LOG = atoi(optarg);
            break;
        }
        case 'a':
        {
            ACTOR_MODEL = atoi(optarg);
            break;
        }
        default:
            break;
        }
    }
}