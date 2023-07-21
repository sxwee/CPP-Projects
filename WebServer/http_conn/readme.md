# HTTP连接处理

本项目的`HTTP`报文处理流程为：

- 客户端（浏览器）发起请求，主线程创建`HTTP`对象接收请求并将该对象插入任务队列（若是`Proactor`模式的话，在插入前会在主线程完成I/O），然后唤醒线程池中的工作线程。
- 工作线程取出请求队列中的任务后，调用`process_read`函数，通过主、从状态机对请求报文进行解析。
- 解析完之后，跳转`do_reques`t函数生成响应报文，通过`process_write`写入`buffer`，返回给浏览器端。

## HTTP报文

HTTP报文分为**请求**报文和**响应**报文。

**请求报文**由以下几个部分组成

- **请求行**：包含请求方法（`GET`、`POST`等）、请求URL和HTTP协议版本。
- **请求头**：包含关于请求的附加信息，例如`HOST`（请求主机）、`ACCEPT`（接受的媒体类型）等。
- **空行**：用于分隔请求头部和请求体。
- **请求体**：包含发送给网页的数据，常用于`POST`请求。

`GET`请求示例如下：

```
GET / HTTP/1.1
Accept: text/html,application/xhtml+xml,application/xml;q=0.9,image/avif,image/webp,image/apng,*/*;q=0.8,application/signed-exchange;v=b3;q=0.7
Accept-Encoding: gzip, deflate
Accept-Language: zh-CN,zh;q=0.9,en-US;q=0.8,en;q=0.7
Cache-Control: max-age=0
Connection: keep-alive
Host: xxx
Upgrade-Insecure-Requests: 1
User-Agent: xxx
空行
请求体 (空)
```

**响应报文**由以下几个部分组成：

- **状态行**：包含HTTP协议版本、状态码和对应的状态消息。
- **响应头部**：类似于请求头部，在响应中携带了关于响应的附加信息，如响应的内容类型、响应时间等。
- **空行**：用于分隔响应头部和响应体。
- **响应体**：包含服务器返回的实际数据，比如HTML页面、图片文件等。

响应报文示例为：

```
HTTP/1.1 200 OK
Content-Length:1827
Connection:keep-alive
空行
<!DOCTYPE html>
<html>
	xxx
</html>
```

## 有限状态机

有限状态机（Finite State Machine，简称FSM）用于描述对象或系统在不同状态之间转换的行为。它由一组状态、状态之间的转移条件和动作组成。

在本项目中包含主、从两个状态机，主状态机内部调用从状态机，从状态机驱动主状态机。

**主状态机**负责解析请求报文的各个部分，其包含的状态有：

- `CHECK_STATE_REQUESTLINE`: 解析请求行。
- `CHECK_STATE_HEADER`: 解析请求头。
- `CHECK_STATE_CONTENT`: 解析消息体，仅用于解析POST请求（仅`Content-length`值不为0时才会进入改状态）。

**从状态机**负责读取报文的一行，项目中采用三种状态表示解析一行的读取状态：

- `LINE_OK`: 完整读取一行。
- `LINE_BAD`: 报文语法错误。
- `LINE_OPEN`: 读取的行不完整。

