
## 测试环境

    VMware
    RHEL 7.5
    Intel(R) Core(TM) i5-6200U CPU @ 2.30GHz

## 测试注意

测试开始时，确保计算机当前 CPU 占用率不是太高，最好 5% 左右。
最好只开虚拟机测试，其他程序全部关掉。

## 测试记录

- 第一次测试

| 测试次数 | 完整处理率 | 每秒事务数TPS |   
|:-------:|:---------:|:-------------:|
|  10000  |   100%    |      2122     |
|  10000  |   100%    |      1853     |
|  10000  |   100%    |      2168     |

- 第二次对比测试

在夜深人静的时候，我趁着 cpu 不繁忙，又悄悄做了一次对比测试，测试的参与者是 std::queue with std::mutex 和 moodycamel::concurrentqueue，现记录如下:

std::queue with std::mutex 测试:

| 测试次数 | 完整处理率 | 每秒事务数TPS |   
|:-------:|:---------:|:-------------:|
|  10000  |   100%    |      2507     |
|  10000  |   100%    |      2936     |
|  10000  |   100%    |      2538     |
|  10000  |   100%    |      2541     |
|  10000  |   100%    |      2726     |
|  10000  |   100%    |      2672     |
|  10000  |   100%    |      2619     |
|  10000  |   100%    |      2877     |

moodycamel::concurrent

| 测试次数 | 完整处理率 | 每秒事务数TPS |
|:-------:|:---------:|:-------------:|
|  10000  |   100%    |      2815     |
|  10000  |   100%    |      2816     |
|  10000  |   100%    |      2392     |
|  10000  |   100%    |      1827     |
|  10000  |   100%    |      2583     |
|  10000  |   100%    |      2498     |
|  10000  |   100%    |      2381     |
|  10000  |   100%    |      2520     |

虽然网上有人[测试](https://blog.csdn.net/zieckey/article/details/69803011)说 moodycamel::concurrent 效率远远优于 std::queue with std::mutex, 但我这里测试的结果相差不多，反而后者更胜一点。

暂时不太清楚怎么回事，等有空把线程队列换成 moodycamel::BlockingWriteReadQueue 再试一下。

在测试 std::queue with std::mutex 时，某一次启动时发生了 double free ，看了一下是开启某个子进程造成的（这之后又重新调起来了...），到目前为止也只出现了这一次，这里只作一下记录。
但这并不能说明 moodycamel::concurrent 一定比 std::queue with std::mutex 要稳定。
程序变复杂了，可能常常会伴随着难以捉摸的异常，有的时候不得不吐吐舌头，暂时作罢...

- 第三次测试

解放生产力，不使用那个烂烂的同步日志了...

| 测试次数 | 完整处理率 |  每秒事务数TPS  |
|:-------:|:---------:|:--------------:|
|  10000  |   100%    |      10757     |
|  10000  |   100%    |      10232     |
|  10000  |   100%    |      10287     |
|  10000  |   100%    |      10055     |
|  10000  |   100%    |      12229     |
|  10000  |   100%    |      11141     |
|  10000  |   100%    |      11376     |
|  10000  |   100%    |      9949      |