# 一.网络应用

> Networked Applications

网络应用可以在世界范围内交换数据，例如你可以通过浏览器读取出版社服务器提供的文章。

网络应用的基本模型：两台主机各自在本地运行一个程序，程序通过网络来通信。

![1-1-networked-application](./Images/1-1-networked-application.png)

最常用的通信模型使用**双向可靠字节流**，通信的两台主机可以想对方发送数据或读取对方发送过来的数据，双方也都可以主动断开连接。

## 典型的网络应用

**World Wide Web（万维网）**

万维网通过**HTTP（HyperText Transfer Protocol）** 工作。在HTTP中，客户端与服务器间建立连接，然后发送请求。例如通过`GET`请求斯坦福大学的主页`http://www.stanford.edu/`，在该过程中，首先浏览器需要与服务器`www.stanford.edu`建立连接，然后发送`GET`请求，然后服务器接收请求是否合法，用户是否能获取，然后回复响应。响应会带有一个数字码，例如`200`表示响应成功，`404`表示没找到用户请求的文件。

------

**BitTorrent**

BitTorrent允许共享和交换大文件。在BitTorrent中，客户端从其他客户端请求文档，因此**单个客户端可以并行地向许多其他客户端请求数据**。

BitTorrent将文件切分成**块（pieces）**，当一个客户端从另一个客户端下载了一个完整的块，该客户端随后会告知其他客户端其上有这个块，其他客户端可以来下载。这些协作的客户端称之为**集群（swarms）**。

与WWW一样，BitTorrent同样使用可靠、双向的数据流，但它更为复杂。当一个客户端想下载一个文件，其首先需要找到称之为torrent文件，该文件描述了你想要下载的文件的某些信息。它还告诉BitTorrent谁是该 torrent 的**跟踪器（Tracker）**。跟踪者记录了谁是集群的成员。为了加入一个torrent，客户端首先需要与tracker通信，请求集群的客户端列表。然后客户端本地与客户端列表的某些建立连接并请求数据。反过来，这些客户端同样也可以向本地客户端请求文件。此外，当有新客户端加入集群时，tracker会告知本地客户端。

![1-2-bittorrent](./Images/1-2-bittorrent.png)

------

**Skype**

Skype提供语音、聊天和视频服务。与万维网在客户端和服务器间建立连接通信不同，Skype是在两台客户端间建立连接（个人PC）。例如下图，两台客户端建立连接然后就可以进行双向的数据交换。

![1-3-skype](./Images/1-3-skype.png)

但当引入NAT（Network Address Translator，网络地址转换器）之后，两台客户端通过Skype通信将有所不同。

> NAT可以将内网IP转换为公网IP，NAT后的内网主机可以打开与Internet的请求，但Internet上的其他节点却不能主动地打开到NAT后主机的连接。

**只有一端有NAT的情况**：通过**Rendezvous服务器**来解决。当登录到Skype，客户端B会建立与Rendezvous服务器的连接（NAT后的主机可以打开与Internet节点的连接），因此当客户端A呼叫客户端B时，它将发送消息给Rendezvous服务器。客户端B上弹出呼叫对话框。若客户端B接受了呼叫，其会主动建立与客户端A的连接。

![1-4-skype-1nat](./Images/1-4-skype-1nat.png)

**两端都位于NAT的情况**：通过**Relay服务器**解决，注意Relay不位于NAT后。位于NAT的客户端A和客户端B都会建立与Relay服务器的连接，当客户端A发送数据时，Relay服务器将该消息转发到其与客户端B建立的连接中。

![1-5-skype-2nat](./Images/1-5-skype-2nat.png)