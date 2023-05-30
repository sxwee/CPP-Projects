/*
 * (C) Radim Kolar 1997-2004
 * This is free software, see GNU Public License version 2 for
 * details.
 *
 * Simple forking WWW Server benchmark:
 *
 * Usage:
 *   webbench --help
 *
 * Return codes:
 *    0 - sucess
 *    1 - benchmark failed (server is not on-line)
 *    2 - bad param
 *    3 - internal error, fork failed
 *
 */
#include "socket.c"
#include <unistd.h>
#include <sys/param.h>
#include <rpc/types.h>
#include <getopt.h>
#include <strings.h>
#include <time.h>
#include <signal.h>

/* values */
volatile int timerexpired = 0;
int speed = 0;
int failed = 0;
int bytes = 0;
/* globals */
int http10 = 1; /* 0 - http/0.9, 1 - http/1.0, 2 - http/1.1 */
/* Allow: GET, HEAD, OPTIONS, TRACE */
#define METHOD_GET 0     // 向服务器请求数据
#define METHOD_HEAD 1    // 与GET类似,但只返回头信息
#define METHOD_OPTIONS 2 // 用于发送一个请求给服务器，以查询支持的查询方法、标头和选项
#define METHOD_TRACE 3   // 用于发送一个带有Max-Forwards标头的请求回显服务器接收到的信息
#define PROGRAM_VERSION "1.5"
int method = METHOD_GET;
int clients = 1;
int force = 0;
int force_reload = 0;
int proxyport = 80;
char *proxyhost = NULL;
int benchtime = 30;
/* internal */
int mypipe[2];
char host[MAXHOSTNAMELEN];
#define REQUEST_SIZE 2048
char request[REQUEST_SIZE];

// option用来在c程序中传递多个参数
// 格式: {const char *name, int has_arg, int *flag, int val}
//    --name: 参数名
//    --has_arg:
//       no_argument: 该参数后面不跟参数值
//       required_argument: 该参数后面一定要跟个参数值
//       optional_argument: 该参数后面可以跟也可以不跟参数值
//    --flag: NULL时getopt_long()返回该选项的字符代码，否则val字段的字符代码存储在flag字段所指向的变量中
//    --val: 选项的字符代码
static const struct option long_options[] =
    {
        {"force", no_argument, &force, 1},
        {"reload", no_argument, &force_reload, 1},
        {"time", required_argument, NULL, 't'},
        {"help", no_argument, NULL, '?'},
        {"http09", no_argument, NULL, '9'},
        {"http10", no_argument, NULL, '1'},
        {"http11", no_argument, NULL, '2'},
        {"get", no_argument, &method, METHOD_GET},
        {"head", no_argument, &method, METHOD_HEAD},
        {"options", no_argument, &method, METHOD_OPTIONS},
        {"trace", no_argument, &method, METHOD_TRACE},
        {"version", no_argument, NULL, 'V'},
        {"proxy", required_argument, NULL, 'p'},
        {"clients", required_argument, NULL, 'c'},
        {NULL, 0, NULL, 0}};

/* prototypes */
static void benchcore(const char *host, const int port, const char *request);
static int bench(void);
static void build_request(const char *url);

static void alarm_handler(int signal)
{
   timerexpired = 1;
}

static void usage(void)
{
   fprintf(stderr,
           "webbench [option]... URL\n"
           "  -f|--force               Don't wait for reply from server.\n"
           "  -r|--reload              Send reload request - Pragma: no-cache.\n"
           "  -t|--time <sec>          Run benchmark for <sec> seconds. Default 30.\n"
           "  -p|--proxy <server:port> Use proxy server for request.\n"
           "  -c|--clients <n>         Run <n> HTTP clients at once. Default one.\n"
           "  -9|--http09              Use HTTP/0.9 style requests.\n"
           "  -1|--http10              Use HTTP/1.0 protocol.\n"
           "  -2|--http11              Use HTTP/1.1 protocol.\n"
           "  --get                    Use GET request method.\n"
           "  --head                   Use HEAD request method.\n"
           "  --options                Use OPTIONS request method.\n"
           "  --trace                  Use TRACE request method.\n"
           "  -?|-h|--help             This information.\n"
           "  -V|--version             Display program version.\n");
};

int main(int argc, char *argv[])
{
   int opt = 0;
   int options_index = 0;
   char *tmp = NULL;

   if (argc == 1)
   {
      usage();
      return 2;
   }

   // getopt_long用于解析命令行选项，该函数一次只能解析一个参数，并返回下一个选项的字符代码
   //    --optstring: 短选项字符串, 带冒号表示选项需要参数
   while ((opt = getopt_long(argc, argv, "912Vfrt:p:c:?h", long_options, &options_index)) != EOF)
   {
      switch (opt)
      {
      case 0:
         break;
      case 'f':
         force = 1;
         break;
      case 'r':
         force_reload = 1;
         break;
      case '9':
         http10 = 0;
         break;
      case '1':
         http10 = 1;
         break;
      case '2':
         http10 = 2;
         break;
      case 'V':
         printf(PROGRAM_VERSION "\n");
         exit(0);
      case 't':
         benchtime = atoi(optarg);
         break;
      case 'p':
         /* proxy server parsing server:port */
         // optarg是指向当前选项参数的指针
         // 获取端口号
         tmp = strrchr(optarg, ':');
         proxyhost = optarg;
         if (tmp == NULL)
         {
            break;
         }
         if (tmp == optarg)
         {
            fprintf(stderr, "Error in option --proxy %s: Missing hostname.\n", optarg);
            return 2;
         }
         if (tmp == optarg + strlen(optarg) - 1)
         {
            fprintf(stderr, "Error in option --proxy %s Port number is missing.\n", optarg);
            return 2;
         }
         *tmp = '\0';
         proxyport = atoi(tmp + 1);
         break;
      case ':':
      case 'h':
      case '?':
         usage();
         return 2;
         break;
      case 'c':
         clients = atoi(optarg);
         break;
      }
   }

   // optind等于argc说明没有剩余未处理的命令行参数, 说明缺失了URL
   if (optind == argc)
   {
      fprintf(stderr, "webbench: Missing URL!\n");
      usage();
      return 2;
   }

   if (clients == 0)
      clients = 1;
   if (benchtime == 0)
      benchtime = 60;
   /* Copyright */
   fprintf(stderr, "Webbench - Simple Web Benchmark " PROGRAM_VERSION "\n"
                   "Copyright (c) Radim Kolar 1997-2004, GPL Open Source Software.\n");
   build_request(argv[optind]);
   /* print bench info */
   printf("\nBenchmarking: ");
   switch (method)
   {
   case METHOD_GET:
   default:
      printf("GET");
      break;
   case METHOD_OPTIONS:
      printf("OPTIONS");
      break;
   case METHOD_HEAD:
      printf("HEAD");
      break;
   case METHOD_TRACE:
      printf("TRACE");
      break;
   }
   printf(" %s", argv[optind]);
   switch (http10)
   {
   case 0:
      printf(" (using HTTP/0.9)");
      break;
   case 2:
      printf(" (using HTTP/1.1)");
      break;
   }
   printf("\n");
   if (clients == 1)
      printf("1 client");
   else
      printf("%d clients", clients);

   printf(", running %d sec", benchtime);
   if (force)
      printf(", early socket close");
   if (proxyhost != NULL)
      printf(", via proxy server %s:%d", proxyhost, proxyport);
   if (force_reload)
      printf(", forcing reload");
   printf(".\n");
   return bench();
}

void build_request(const char *url)
{
   char tmp[10];
   int i;

   bzero(host, MAXHOSTNAMELEN);
   bzero(request, REQUEST_SIZE);

   if (force_reload && proxyhost != NULL && http10 < 1)
      http10 = 1;
   if (method == METHOD_HEAD && http10 < 1)
      http10 = 1;
   if (method == METHOD_OPTIONS && http10 < 2)
      http10 = 2;
   if (method == METHOD_TRACE && http10 < 2)
      http10 = 2;

   // 设置请求的类型
   switch (method)
   {
   default:
   case METHOD_GET:
      strcpy(request, "GET");
      break;
   case METHOD_HEAD:
      strcpy(request, "HEAD");
      break;
   case METHOD_OPTIONS:
      strcpy(request, "OPTIONS");
      break;
   case METHOD_TRACE:
      strcpy(request, "TRACE");
      break;
   }

   strcat(request, " ");
   // 检验URL是否合法
   if (NULL == strstr(url, "://"))
   {
      fprintf(stderr, "\n%s: is not a valid URL.\n", url);
      exit(2);
   }
   // 检验URL的长度是否超出限制
   if (strlen(url) > 1500)
   {
      fprintf(stderr, "URL is too long.\n");
      exit(2);
   }
   if (proxyhost == NULL)
      if (0 != strncasecmp("http://", url, 7)) // 检验是否未HTTP请求
      {
         fprintf(stderr, "\nOnly HTTP protocol is directly supported, set --proxy for others.\n");
         exit(2);
      }
   // 获取http://x中x的位置
   i = strstr(url, "://") - url + 3;

   if (strchr(url + i, '/') == NULL)
   {
      fprintf(stderr, "\nInvalid URL syntax - hostname don't ends with '/'.\n");
      exit(2);
   }
   if (proxyhost == NULL)
   {
      /* get port from hostname */
      // 若指定了端口
      if (index(url + i, ':') != NULL && index(url + i, ':') < index(url + i, '/'))
      {
         strncpy(host, url + i, strchr(url + i, ':') - url - i);
         bzero(tmp, 10);
         strncpy(tmp, index(url + i, ':') + 1, strchr(url + i, '/') - index(url + i, ':') - 1);
         proxyport = atoi(tmp);
         if (proxyport == 0)
            proxyport = 80;
      }
      else
      {
         strncpy(host, url + i, strcspn(url + i, "/"));
      }
      // 请求方法+请求的路径
      // 示例: GET /posts/webbench-source-and-analysis/
      strcat(request + strlen(request), url + i + strcspn(url + i, "/"));
   }
   else
   {
      strcat(request, url);
   }
   if (http10 == 1)
      strcat(request, " HTTP/1.0");
   else if (http10 == 2)
      strcat(request, " HTTP/1.1");
   strcat(request, "\r\n");
   if (http10 > 0)
      strcat(request, "User-Agent: WebBench " PROGRAM_VERSION "\r\n");
   // 添加HOST字段
   if (proxyhost == NULL && http10 > 0)
   {
      strcat(request, "Host: ");
      strcat(request, host);
      strcat(request, "\r\n");
   }
   // 设置要求服务器不适用缓存
   if (force_reload && proxyhost != NULL)
   {
      strcat(request, "Pragma: no-cache\r\n");
   }
   // HTTP1.1之后采用了持久连接技术, 这里强制设置为短连接
   if (http10 > 1)
      strcat(request, "Connection: close\r\n");
   // HTTP报文用空行分隔head和payload
   if (http10 > 0)
      strcat(request, "\r\n");
}

static int bench(void)
{
   int i, j, k;
   pid_t pid = 0;
   FILE *f;

   // host或代理host创建相应的套接字测试是否连通
   i = Socket(proxyhost == NULL ? host : proxyhost, proxyport);
   if (i < 0)
   {
      fprintf(stderr, "\nConnect to server failed. Aborting benchmark.\n");
      return 1;
   }
   close(i);
   // 创建管道, 调用成功时会将两个文件描述符分别存储到fd[0]和fd[1]中
   // fd[0]代表管道的读端，fd[1]代表管道的写端。这两个文件描述符可以用于父进程和子进程之间进行数据通信
   if (pipe(mypipe))
   {
      perror("pipe failed.");
      return 3;
   }

   /* not needed, since we have alarm() in childrens */
   /* wait 4 next system clock tick */
   /*
   cas=time(NULL);
   while(time(NULL)==cas)
         sched_yield();
   */

   // 根据设置的客户端的数量调用fork函数创建相应数据的子进程
   for (i = 0; i < clients; i++)
   {
      pid = fork();
      // 小于0表示进程创建失败, 等于0表示当前是子进程
      if (pid <= (pid_t)0)
      {
         /* child process or error*/
         sleep(1); /* make childs faster */
         break;
      }
   }

   // 进程创建失败
   if (pid < (pid_t)0)
   {
      fprintf(stderr, "problems forking worker no. %d\n", i);
      perror("fork failed.");
      return 3;
   }

   // 子进程执行的代码
   if (pid == (pid_t)0)
   {
      /* I am a child */
      if (proxyhost == NULL)
         benchcore(host, proxyport, request);
      else
         benchcore(proxyhost, proxyport, request);

      // 子进程将测试的结果发送给父进程
      f = fdopen(mypipe[1], "w");
      if (f == NULL)
      {
         perror("open pipe for writing failed.");
         return 3;
      }
      /* fprintf(stderr,"Child - %d %d\n",speed,failed); */
      fprintf(f, "%d %d %d\n", speed, failed, bytes);
      fclose(f);
      return 0;
   }
   // 父进程执行的代码
   else
   {
      // 读取子进程的测试结果并进行统计并展示
      f = fdopen(mypipe[0], "r");
      if (f == NULL)
      {
         perror("open pipe for reading failed.");
         return 3;
      }
      setvbuf(f, NULL, _IONBF, 0);
      speed = 0;
      failed = 0;
      bytes = 0;

      while (1)
      {
         pid = fscanf(f, "%d %d %d", &i, &j, &k);
         if (pid < 2)
         {
            fprintf(stderr, "Some of our childrens died.\n");
            break;
         }
         speed += i;
         failed += j;
         bytes += k;
         /* fprintf(stderr,"*Knock* %d %d read=%d\n",speed,failed,pid); */
         if (--clients == 0)
            break;
      }
      fclose(f);
      // 请求次数、成功率、平均响应时间
      printf("\nSpeed=%d pages/min, %d bytes/sec.\nRequests: %d susceed, %d failed.\n",
             (int)((speed + failed) / (benchtime / 60.0f)),
             (int)(bytes / (float)benchtime),
             speed,
             failed);
   }
   return i;
}

/*
   在给定的时间内不断的创建连接并发送请求, 期间会统计:
      --speed: 成功的请求数
      --faild: 失败的请求书
      --bytes: 成功收到的响应字节数
*/
void benchcore(const char *host, const int port, const char *req)
{
   int rlen;
   char buf[1500];
   int s, i;
   struct sigaction sa;

   // 设置SIGALRM信号的处理程序
   sa.sa_handler = alarm_handler;
   sa.sa_flags = 0;
   // 注册信号处理函数
   if (sigaction(SIGALRM, &sa, NULL))
      exit(3);
   // 设置定时函数, 超时产生SIGALRM信号
   alarm(benchtime);

   rlen = strlen(req);
nexttry:
   while (1)
   {
      if (timerexpired)
      {
         if (failed > 0)
         {
            /* fprintf(stderr,"Correcting failed by signal\n"); */
            failed--;
         }
         return;
      }
      // 创建套接字
      s = Socket(host, port);
      if (s < 0)
      {
         failed++;
         continue;
      }
      // 发送数据
      if (rlen != write(s, req, rlen))
      {
         failed++;
         close(s);
         continue;
      }
      if (http10 == 0)
         if (shutdown(s, 1)) // SHUT_WR:1表示关闭写方向, 但还能接收数据
         {
            failed++;
            close(s);
            continue;
         }
      if (force == 0)
      {
         /* read all available data from socket */
         while (1)
         {
            if (timerexpired)
               break;
            // 读取数据
            i = read(s, buf, 1500);
            /* fprintf(stderr,"%d\n",i); */
            if (i < 0)
            {
               failed++;
               close(s);
               goto nexttry;
            }
            else if (i == 0) // 数据读完
               break;
            else
               bytes += i;
         }
      }
      if (close(s))
      {
         failed++;
         continue;
      }
      speed++;
   }
}
