**官网地址**: [Web Bench](http://home.tiscali.cz/~cz210552/webbench)

# 项目简介

Web Bench是 基于C语言实现的Linux系统下的HTTP压力测试工具，可以模拟大量用户请求网站所需资源，测试服务器在不同并发情况下的负载能力。

Web Bench通过`fork()`函数创建多进程的方式来模拟多个客户端，客户端可以发送`HTTP/0.9-HTTP/1.1`请求，请求类型包括`GET`、`HEAD`、`OPTIONS`、`TRACE`等。

本项目主要是对Web Bench的项目源码进行阅读和剖析，具体详解`socket.c`和`webbench.c`。
