# 🚀 Cluster-Chat-Server (集群聊天服务器)

>基于 C++11 和 Muduo 网络库实现的分布式聊天服务器。
>利用 Nginx 实现 TCP 负载均衡，通过 Redis 的发布-订阅机制解决多机环境下的跨节点通信问题，并使用 MySQL 进行数据持久化。

## 🛠️ 技术栈

语言/工具：C++11、CMake

网络库：Muduo (epoll + 线程池)

负载均衡：Nginx (TCP stream模块)

中间件：Redis (hiredis)、MySQL (C API)

序列化：nlohmann/json

## 💻 编译与运行

1. 环境依赖

需提前安装好以下环境及相关开发库：
>g++, cmake, muduo, nginx, redis-server, libhiredis-dev, mysql-server, libmysqlclient-dev

2. 数据库配置

在 MySQL 中创建名为 chat 的数据库，并建立以下表：
user, friend, allgroup, groupuser, offlinemessage

3. 编译项目

在项目根目录执行：
```bash
mkdir build && cd build
cmake ..
make
```

4. 启动服务
bash
# 启动基础组件
```bash
sudo /usr/local/nginx/sbin/nginx
sudo service mysql start
sudo service redis-server start
```
# 启动多个 ChatServer 服务端节点 (绑定不同端口)
```bash
cd bin/
./chatServer 127.0.0.1 6000
./chatServer 127.0.0.1 6002
```
# 启动客户端测试 (直接连接 Nginx 代理端口，假设为 8000)
```bash
./chatClient 127.0.0.1 8000
```

## 💬 客户端支持的命令

登录成功后可用以下命令交互：

| 命令格式 | 功能说明 | 示例用法 | 
| :--- | :--- | :--- | 
| **`help`** | 显示所有支持的命令列表 | `help` | 
| **`chat:friendid:msg`** | 一对一私聊 | `chat:1001:你好呀` | 
| **`addfriend:friendid`** | 添加好友 | `addfriend:1002` | 
| **`creategroup:name:desc`** | 创建新群组 | `creategroup:C++交流群:讨论技术` | 
| **`addgroup:groupid`** | 加入群组 | `addgroup:1` | 
| **`groupchat:groupid:msg`** | 发送群聊消息 | `groupchat:1:大家好！` | 
| **`loginout`** | 退出当前登录账号 | `loginout` |
