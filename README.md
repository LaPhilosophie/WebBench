
# webbench简介
webbench是一个http服务器的压力测试工具，原理是通过fork模拟出多个子进程对指定url进行请求，并通过管道通信让子进程将得到的数据发送给父进程，从而完成数据的统计，它最多可以模拟3万个并发连接去测试网站的负载能力

这是我写的源码注释仓库：https://github.com/LaPhilosophie/WebBench

# 分析图

![](https://gls.show/image/webbench-workflow.png))

![](https://gls.show/image/Snipaste_2022-08-25_23-42-10.png))

# 运行效果
```
zarathustra@Nietzsche ~/c/WebBench (master) [2]> ./webbench -c 30 -t 10 http://xinanzhijia.xyz/
Webbench - Simple Web Benchmark 1.5
Copyright (c) Radim Kolar 1997-2004, GPL Open Source Software.

Request:
GET / HTTP/1.0
User-Agent: WebBench 1.5
Host: xinanzhijia.xyz


Runing info: 30 clients, running 10 sec.

Speed=1818 pages/min, 3934 bytes/sec.
Requests: 303 susceed, 0 failed.
```


# 处理命令行参数

webbench使用getopt_long函数进行参数的处理，他支持许多参数（如下）

获知命令行参数之后，进行参数的获取，并设置变量字段，进行请求包的构造

短参 | 长参数 | 作用
---|-----|---
-f | --force | 不需要等待服务器响应
-r | --reload | 发送重新加载请求
-t | --time | 运行多长时间，单位：秒"
-p | --proxy server:port | 使用代理服务器来发送请求
-c | --clients | 创建多少个客户端，默认1个"
-9 | --http09 | 使用 HTTP/0.9
-1 | --http10 | 使用 HTTP/1.0 协议
-2 | --http11 | 使用 HTTP/1.1 协议
--get | 使用 GET请求方法
--head | 使用 HEAD请求方法
--options | 使用 OPTIONS请求方法
--trace | 使用 TRACE请求方法
-?/-h | --help | 打印帮助信息
-V | --version | 显示版本号


## getopt()
getopt:
```
#include <unistd.h>

int getopt(int argc, char * const argv[],
           const char *optstring);

extern char *optarg;
extern int optind, opterr, optopt;
```
getopt解析命令行参数，函数原型如上。argc和argv是main函数的参数，argv中以-开头的是选项，比如-v，v是选项
- 如果重复调用 getopt () ，它将依次返回每个选项元素中的每个选项字符
- optstring是一个字符串，比如“nm:sl”，这表示可以处理四个选项，-n、-m、-s、-l，其中，：之前的字母可以有参数，比如在这里，由于：之前有m，所以可以有如下用法：-m xxx
- 变量 optind 是要在 argv 中处理的下一个元素的索引。系统将此值初始化为1
- 如果 getopt ()找到一个选项字符，它会**返回该字符**，更新extend变量 optind 和一个static变量 nextchar
- 解析了所有的命令行选项,getopt () **返回-1**，且optind是 argv 中第一个不是选项的 argv 元素的索引
- 如果遇到不在optstring中的字符，返回？
- 如果 getopt ()遇到一个缺少参数的选项且optstring第一个字符不是:，那么返回？

## getopt_long()
getopt_long：
```
#include <getopt.h>

int getopt_long(int argc, char * const argv[],
           const char *optstring,
           const struct option *longopts, 
           int *longindex);

struct option {
    const char *name;
    int         has_arg;
    int        *flag;
    int         val;
};
```

- getopt_long与getopt类似，但是同时也可以处理长选项，即以两个横杠开头的选项，比如--version
- name类似于上面的optstring
- has_arg：
  - no_argument (or 0)
  - required_argument (or 1)
  - optional_argument (or 2) 
- 如果flag为NULL，则getopt_long()返回val，否则val赋值给flag指针所指内容
- getopt_long的返回值类似于getopt函数（error、-1、？）
- longopts数组的最后一个元素应该全为0
- optarg: 如果合法选项带有参数，那么对应的参数，赋值给optarg
  
关于flag的描述有点绕，摘抄一下：

> specifies how results are returned for a long option. If flag is NULL, then getopt_long() returns val. (For example, the calling program may set val to the equivalent short option character.) Otherwise, getopt_long() returns 0, and flag points to a variable which is set to val if the option is found, but left unchanged if the option is not found.
> 

# 请求包的构造

- 请求报文被存到request数组，它是一个全局变量
- build_request用到了大量的字符串处理函数，比如memset、strcat、strchr、strstr
- 处理之后request数组所储存的内容类似：
```
GET / HTTP/1.0
User-Agent: WebBench 1.5
Host: gls.show
```

# bench函数
该函数是项目的核心

- 先调用socket.c中的socket函数测试是否可以连通
- fork出client数量的进程。注意到这里通过子进程在循环中的break使得不会出现指数爆炸式的进程数量，而是设置的client数量的进程
- 创建管道，父子进程分离，并通过管道进行通信
  - 子进程：连接服务器并收发包得到数据，通过管道写端传送给父进程
  - 父进程：通过管道读端得到数据，并统计得出最终结果
- 通关自定义的信号控制命令行参数中的时间设置

# 后记
- 总体来说还是个不错的项目，但是代码中确实有些写的不够好的地方
  - pid_t同时用于接受fork返回值和fscanf函数返回值
  - 使用了大量的全局变量
  - 没有关闭管道不需要的一端
  - 错误处理有些混乱