# 1.TCP服务模型

Internet上绝大部分应用使用的都是TCP（Transmission Control Protocol），TCP提供**可靠的、端到端的、有序的、双向字节流**服务。

应用会将需要传输的数据传递至TCP，TCP会对数据进行分段，称之为TCP Segment，之后TCP传递segments给IP层，IP层将其封装在IP数据报中。IP数据包随送至Link层进一步封装为Link帧（frame），随后发送出去。

<p align="center"><img src="Images/2-1-TCP-example.png" alt="2-1-TCP-example"></p>

当两个应用使用TCP，它们间会建立双向通信通道。首先TCP建立从A到B的通信通道，随后建立从B到A的通信通道。这样的双向通信称之为**连接（connection）**。在连接的两端，TCP 维护一个状态机以跟踪连接的状态。

<p align="center"><img src="Images/2-2-TCP-Communication.png" alt="2-2-TCP-Communication"></p>



## 1.1 三次握手

TCP通过**三次握手（three-way handshake）**来建立主机A和B之间的连接。

- **第一次握手**：首先主机A发送建立连接请求（SYN message，SYN是Synchronize的简写）。在SYN message中还包含了主机A的序列号，即通信的起始报文序列号。
- **第二次握手**：主机B响应SYN+ACK。主机B对主机A的请求恢复ACK，表示同意建立从A到B的通信。此外，主机B同样发送SYN请求建立从B到A的通信，主机B同样也会告知对方通信的起始报文序列号。
- **第三次握手**：A响应ACK，表示同意建立从B到A的反向连接。

经过三次握手，双方便建立了双向连接，之后便可以进行数据交换了。

<p align="center"><img src="Images/2-3-TCP-three-way-handshake.png" alt="2-3-TCP-three-way-handshake"></p>

## 1.2 四次挥手

TCP通过**四次挥手**来关闭通信双方的连接。

- **第一次挥手**：主机A上的TCP层通过发送一个FIN（FINISH的缩写）消息来关闭连接。
- **第二次挥手**：主机B对FIN消息进行确认，即A不再有数据发送过来，关闭从A到B的数据流。但B可能仍然有数据需要发送给A，此时B不会立即请求关闭从B到A的通道。因此，此时B仍然能发送数据给A。
- **第三次挥手**：当B完成将数据发送给A后，同样向A发送FIN消息，请求关闭从B到A方向的连接。
- **第四次挥手**：主机A收到FIN请求后会回复ACK，然后关闭从B到A方向的连接，至此TCP连接被彻底关闭。

<p align="center"><img src="Images/2-4-TCP-connect-teardown.png" alt="2-4-TCP-connect-teardown"></p>

## 1.3 TCP服务属性

下表总结了TCP提供的服务

<p align="center"><img src="Images/2-5-TCP-Service.png" alt="2-5-TCP-Service"></p>

表中前3个服务是TCP为应用程序提供的服务，也就是可靠的字节流服务。

**可靠性**指数据被正确传送，TCP使用4个机制来保证可靠性：

- 当TCP层接收到数据时，它会发送一个**确认（acknowledgement）**给发送方，以通知其数据已经正确到达。
- TCP头携带一个覆盖段内头部和数据的**校验和（checksums）**。校验和的作用是检测段是否在传输过程中被损坏，例如由于电线上的位错误或路由器内存故障等原因。
- **序列号**可以检测数据的确实。每个TCP段的头部携带了该段内第一个字节在字节流中的序列号。若某个段丢失了，将不会收到对方的ACK确认。若确认丢失，发送方需要重新发送数据。
- **流量控制（Flow-control）**防止接收方超负荷。如果主机A比主机B快得多，那么主机A可能会通过发送数据过快而淹没主机B，此时若主机A继续发送，数据将会被丢弃。TCP使用流量控制来防止这种事情发生，具体做法是接收方不断告知发送方自己的缓冲区还有多少空间能接收新数据。

TCP的**有序性**也是通过序列号来保证的。

**拥塞控制（controlling congestion）**：TCP试图在使用网络时在所有TCP连接之间平均分配网络容量。

## 1.4 TCP段格式

TCP段格式如下所示

<p align="center"><img src="Images/2-6-TCP-segment-format.png" alt="2-6-TCP-segment-format"></p>

由于TCP需要提供双向可靠、有序的字节流服务，因此其报头要比IP包头复杂得多。

| 字段                             | 说明                                                         |
| -------------------------------- | ------------------------------------------------------------ |
| `Destination port`               | 通信对端应用的端口，例如HTTP服务端口为80、SSH服务端口为22    |
| `Source port`                    | 本端应用的端口                                               |
| `Sequence number`                | TCP数据字段中第一个字节在字节流中的位置                      |
| `Acknowledgment sequence number` | 告知对端下一个期望收到的字节序列号，同时也表明该序列号之前的数据已经被成功接收 |
| `Checksum`                       | 在整个段上计算的校验和，用来帮助接收方检查数据是否缺损       |
| `Header Length`                  | 包头的长度，由于有`Options`字段选项，TCP包头可能会变化       |
| `Flags`                          | 一系列标志位，其中`ACK`表示确认号是否有效，`PSH`表示是否应该尽快将数据交给应用层，`RST`表示连接复位请求，`SYN`表示同步序列号请求，`FIN`表示发送方已经没有数据需要传送，并要求释放连接 |

## 1.6 TCP连接的标识ID

TCP连接在TCP和IP头中由五个信息唯一标识，即**源IP地址、目的IP地址、协议ID（是TCP还是UDP）、源Port和目的Port**。

> 也有说TCP连接是用四元组标识的，即去掉上面的协议ID。

端口号包含16位，因此客户端最多能与服务端的同一个应用建立64K新连接，若超过了则会出现重复的端口。如果发生这种情况，一个连接的字节可能会与另一个连接的字节混淆。为了降低混淆的可能性，TCP连接会使用随机初始序列号来指代字节流中的字节，这样能做一定程度上避免这种混淆。

## 1.8 序列号

TCP segment的序列号指该段中数据的第一个字节的序列号，该序列号是初始序列号偏移后得到的。TCP的确认序列号指期望收到的下一个字节的序列号。

<p align="center"><img src="Images/2-7-sequence-number.png" alt="2-7-sequence-number"></p>

# 2.UDP服务模型

**UDP（User Datagram Protocol）**是另一个传输层协议。UDP被用于不需要TCP保证传递服务的应用程序，这可能是因为该应用程序使用自己的私有方式处理重传，或者仅仅因为该应用程序不需要可靠的传递。UDP要比TCP简单得多。

> DNS便是使用的UDP进行通信，当DNS客户端向DNS服务器发送查询请求时，通常使用UDP协议进行传输。这是因为DNS查询通常很小，可以适应UDP数据报的限制，并且在大多数情况下，速度和效率比TCP更高。

## 2.1 UDP数据报格式

UDP数据报格式如下所示

<p align="center"><img src="Images/2-8-UDP-datagram-format.png" alt="2-8-UDP-datagram-format"></p>

UDP包头字段含义如下

| 字段               | 说明                                                         |
| ------------------ | ------------------------------------------------------------ |
| `Source port`      | 指示数据来自哪个应用程序                                     |
| `Destination Port` | 指示数据需要发送给对端主机的哪个应用                         |
| `Chechsum`         | 校验和，可选字段，如果发送方不包括校验和，则该字段填充全零。否则它将计算UDP头和数据上的校验和 |
| `Length`           | 指整个UDP数据报的字节长度                                    |

事实上，UDP校验和计算也**包含了IPv4头部的一部分（源IP、目的IP和协议ID）**。这样做实际违反了分层原则，但这样可以使得UDP层可以检测到被传递到错误目标的数据报。

## 2.2 UDP服务属性

下表总结了UDP的属性

<p align="center"><img src="Images/2-9-UDP-Service.png" alt="2-9-UDP-Service"></p>

UDP提供**无连接（conectionless）** 的数据报服务。

UDP的数据包是**自包含的（self-contained） **。

UDP是**不可靠（unreliable） **的数据传送服务。UDP不发送任何确认消息来告知数据已到达另一端。它没有检测丢失数据报的机制。如果整个数据报在传输途中被丢弃，UDP将不会通知应用程序，并且也不会要求源重新发送数据报。



