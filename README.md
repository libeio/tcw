
## 开源支持
- [rapidjson](https://github.com/Tencent/rapidjson)
- [tortellini](https://github.com/Qix-/tortellini)

## 项目管理、编译及运行环境
- 项目管理工具支持
    + [x] make (为加快编译，会生成一个静态库。建议调试时使用)
    + [x] cmake (源文件编译)
- 最小编译配置支持
    + [x] GNU gcc/g++ 4.8.4
    + [x] cmake 3.0.2
- 运行过的机器环境
    + [x] Kylin 4.0.2 (aarch64)
    + [x] Ubuntu 14.04 (x86_64)
    + [x] RHEL 7.5 (x86_64)
    + [x] Debian 9.8 (x86_64)

## 处理
- [x] 编译警告处理
- [x] 内存泄漏处理
    + 无内存泄漏
- [x] 日志进程间隔离处理
    + 当前进程内日志只记录当前进程内信息，不会造成进程间异步打印事故
- [x] 信号处理
    + 暂只对一些常见信号进行处理
- [ ] 服务可配置化处理
    + 地址端口可配置
    + 服务名称可配置
    + 日志等级可配置

## 结构说明
- 主要包括四个部分，分别是消息解析、消息定义、事件反应、事件处理。其文件结构如下:
    + 消息解析
        + rapidjson\
        + raj.h
    + 消息定义
        + bic_type.h
        + bic.h
        + bic.cpp
    + 事件反应
        + eeclient.h
        + eeclient.cpp
        + eehandler.h
        + eehandler.cpp
    + 事件处理
        + eemodule.h
        + eemodule.cpp
  另外还有一些辅助文件。
- 消息解析
    + 为方便网络传输，这里通过 json 的方式对信息或数据进行序列化或结构化；
    + json 使用开源的 rapidjson, 不会生成额外的依赖库；
    + 为了方便使用，对 rapidjson 进行简单的模板化封装。
- 消息定义
    + 每种消息采用 class 定义，名称为 BIC_??? 。消息类内成员使用 rapidjson 进行序列化或结构化；
    + 从应用层角度看，消息一共分成两层，分别是自定义信息头和消息体，后者又包括消息头和有效荷载体。
        + 为解决 tcp 传输处理过程中的一些问题，定义了自定义信息头，其内容包括版本号，消息体长度，
          消息校验值等内容；
        + 为了方便消息的定向传输及消息类型获取，将消息体分成了两个部分，分别是消息头和有效荷载体。
          消息头主要定义了消息的流向、类型和诞生时间，而有效荷载体则定义具体的业务内容；
    + 整个消息包结构如下(使用 json 表示):
        ```json
        Packet: {
            NegoHeader: {
                // ...
            },
            BIC_MESSAGE: {
                BIC_HEADER : {
                    // ...
                },
                BIC_PAYLOAD : {
                    // ...
                }
            }
        }
        ```
- 事件反应
    + 事件类型包括读、写、连接、关闭或异常等；
    + 通过 epoll 处理描述符(包括管道符和套接字)方式来处理相关事件并做出相应响应, epoll 通过
      EpollEvHandler 类进行封装；
    + 为了方便对每个描述符的维护和管理，并适时记录一些信息，将描述符封装入 EClient 结构中；
    + linux 下，常见的描述符大致有四类，分别是 tcp套接字、管道符、udp套接字、普通文件套接字。
      其中前两种是面向连接的描述符，后两种是面向非连接的描述符，本处理暂只支持面向连接的描述
      符；
    + 事件反应部分只作简单的读、写及连接与关闭等，具体的业务处理由事件处理部分完成；
    + 事件反应还会对可能出现的僵尸进程进行处理；
- 事件处理
    + 事件处理采用同步回调处理方式；
    + 所负责的具体业务主要包括 消息校验、消息序列化及结构化、心跳、定时器、信号处理、fork 子
      进程、进程杀灭、子进程重新调起等；
    + 具体见功能说明；

## 功能说明
为便于描述，将下发策略的远程服务端称为策略端，本地守护子进程服务的进程称为守护端，用于示例的
两个子进程服务分别称为魔偶甜点端和机关傀儡端。
- 消息校验
    + 对消息体部分(BIC_MESSAGE)进行校验；
    + 使用较为常用的 crc32 作为校验方法；
    + 因为对 crc32 校验原理不是太了解，所以校验方式是对接收数据重新计算 crc32，将结果进行比对；
- 消息序列化及结构化
    + 调用 bic.h 文件中定义的消息类方法对消息进行处理；
- 心跳
    + 方式: 客户端主动发送心跳，服务端通过对自己所负责的客户端进行定期检查来判断连接是否丢失；
    + 这种方式可以减少一次消息传输，且处理起来也较为简单；
    + 具体就是魔偶甜点端和机关傀儡端向守护端发送心跳，守护端向策略端发送心跳；  
- 定时器
    + 通过设置 epoll 超时实现伪定时器；
    + 也可以通过异步触发 epoll 监控事件或其他方式来实现(真定时器)，但应用于该场景中情况比较
      复杂，较难控制，不推荐；
- 信号处理
    + 对于 SIGPIPE 进行忽略处理；
    + 为了能够优雅的杀死子进程，定义了 SIGALRM 进行处理；
    + 为了方便子进程的重新调起，重定义了 SIGINT 的处理方式(当然，也可以采用其他信号)；
    + 当前还对实时队列信号([SIGRTMIN, SIGRTMAX])进行了屏蔽处理，但发现毫无必要，后续可
      将其去掉；
    + 对于具体的服务线程，后续可以对一些信号，如 SIGSEGV, SIGBUS 等进行屏蔽处理，但应该也没
      有必要；
- fork 子进程和子进程重新调起
    + 为了方便维护管理以及 fork 时对堆栈的处理，子进程重新调起和 fork 子进程使用同样的处理方
      式；
    + 具体就是守护端在一段时间内没有接收到来自魔偶甜点端或机关傀儡端的心跳后，会重新 fork 一
      个进程并清空守护端历史遗留堆栈，方便后续为魔偶甜点或机关傀儡创建堆栈；
- 进程杀灭
    + 可以通过下发消息的方式杀死某个子进程；
    + 具体就是策略端将杀死机关傀儡端的消息发送给守护端，守护端再转给机关傀儡端，后者收到此消
      息后自毁(exit)；
    + 可能还会存在不属于某个进程的子进程，但却是在该进程内启动的进程。对于这类进程，可通过信
      号方式使其退出；
    + 进程杀灭一般很少使用，目前只在测试情况下使用；

## 消息队列说明
- 消息队列机制
    + 进程间的读写是异步的，如果读端较慢而写端较快，写端可能会:
        1.读端还未开始读取，写端又重写缓冲区，覆盖之前的数据，造成读端读取消息有遗漏；
        2.读端仅读取一部分数据，写端缓冲区仍有另外一部分数据，此时新消息重写写端缓冲区，造成
          读端读取消息会拼错；
    + 为了避免上述情况，采用消息队列机制。
- 消息延迟计算
    + 理想情况下，写端写入一个消息到队列后，读端会立即从队列中取出并处理，消息并不会在队列中
      "停留"太长时间。实际情况可能是，一个消息加入到队列后，读端还未处理，此时另外的消息也加
      入到队列中，这种情况下，需要对消息延迟进行观察，以判断延迟程度，进而估计系统性能；
    + 理想情况下，消息流空闲时，队列多数时间为空，消息队列最大长度不会超过1；消息流繁忙时，
      队列最大长度也不会超过1。所以可以简单的通过消息队列长度来判断消息延迟；
        1. 消息流空闲时: 消息延迟占比 = 消息队列长度为1 / (消息队列长度为0 + 消息队列长度为1)
        2. 消息流繁忙时: 消息延迟 = 消息队列长度 - 1
      消息流繁忙时，可自定义一个低延迟标准(比如 3以下)，之后据此计算一段时间内超过该标准的占比。
    + 主要是对消息流繁忙时的消息延迟进行统计；

## 大文件传输说明
- 文件采用分块传输方式，块与块之间顺序性独立发送(放入队列后立即发送)，与将整个文件切片放入队列后
  统一发送相比，优点如下:
    + 减小传输队列，节省内存；
    + 收发异步，节省时间；
    + 有效减少校验错误；
    + 块与块之间独立发送，某块出错后从出错块开始重传，较为节省资源和时间(暂未实现，需要微调结构)；
- 目前只支持并行发送一个文件，可根据需要实现支持多文件并行发送，但目前还无必要，也暂未实现；
- 分块传输时，因为 tcp 已经进行了确认机制，所以这里收端不再对每块数据向发端发送确认，只在校验出
  错时发送重传消息，以提高效率；
- 虽然没有实现重传，但目前为止对一些文件的传输测试(10M, 20M, 50M, 100M, 300M)暂未出错过；

## 关于 EClient 的说明
- 为方便后续扩展，可将 EClient 定义为纯虚函数；
- 可为 EClient 添加一些虚函数，如数据库处理函数或结构化序列化函数，以便将 EClient 状态数据写入本
  地数据库或上传至远端服务器；
  
## 欠缺说明
- 对进程内启动进程的处理；
- 文件的归档和压缩；

## 注意说明
- 不支持多线程；
- 创建子进程时要注意对父进程历史遗留堆栈数据的清理；

## 版本说明

## 其他
- 为 eehandler 添加子进程服务接口成员函数