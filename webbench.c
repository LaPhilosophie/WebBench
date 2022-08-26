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
volatile int timerexpired=0;
int speed=0;
int failed=0;
int bytes=0;

/* globals */
int http10=1; /* 0 - http/0.9, 1 - http/1.0, 2 - http/1.1 */
/* Allow: GET, HEAD, OPTIONS, TRACE */
#define METHOD_GET 0
#define METHOD_HEAD 1
#define METHOD_OPTIONS 2
#define METHOD_TRACE 3
#define PROGRAM_VERSION "1.5"
int method=METHOD_GET;
int clients=1;
int force=0;
int force_reload=0;
int proxyport=80;
char *proxyhost=NULL;
int benchtime=30;

/* internal */
int mypipe[2];
char host[MAXHOSTNAMELEN];
#define REQUEST_SIZE 2048
char request[REQUEST_SIZE];

static const struct option long_options[]=
{
    {"force",no_argument,&force,1},
    {"reload",no_argument,&force_reload,1},
    {"time",required_argument,NULL,'t'},
    {"help",no_argument,NULL,'?'},
    {"http09",no_argument,NULL,'9'},
    {"http10",no_argument,NULL,'1'},
    {"http11",no_argument,NULL,'2'},
    {"get",no_argument,&method,METHOD_GET},
    {"head",no_argument,&method,METHOD_HEAD},
    {"options",no_argument,&method,METHOD_OPTIONS},
    {"trace",no_argument,&method,METHOD_TRACE},
    {"version",no_argument,NULL,'V'},
    {"proxy",required_argument,NULL,'p'},
    {"clients",required_argument,NULL,'c'},
    {NULL,0,NULL,0}
};

/* prototypes */
static void benchcore(const char* host,const int port, const char *request);
static int bench(void);
static void build_request(const char *url);

static void alarm_handler(int signal)
{
    timerexpired=1;
}	

static void usage(void)//输出用法
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
            "  -V|--version             Display program version.\n"
           );
}

int main(int argc, char *argv[])
{
    int opt=0;
    int options_index=0;
    char *tmp=NULL;
    
    //如果命令行参数只有一个，跳到usage函数
    if(argc==1)
    {
        usage();
        return 2;
    } 
    //通过getopt_long函数进行参数的获取，并设置变量
    while((opt=getopt_long(argc,argv,"912Vfrt:p:c:?h",long_options,&options_index))!=EOF )
    {
        switch(opt)
        {
            case  0 : break;
            case 'f': force=1;break;
            case 'r': force_reload=1;break; 
            case '9': http10=0;break;
            case '1': http10=1;break;
            case '2': http10=2;break;
            case 'V': printf(PROGRAM_VERSION"\n");exit(0);
            case 't': benchtime=atoi(optarg);break;	     
            case 'p': //如果是-p选项，optarg保存的是形如“server:port”的字符串
            /* proxy server parsing server:port */
            tmp=strrchr(optarg,':');//strrchr函数返回字符串中最后一次出现字符的位置
            proxyhost=optarg;//"ip:port"
            if(tmp==NULL)
            {
                break;
            }
            if(tmp==optarg)
            {
                fprintf(stderr,"Error in option --proxy %s: Missing hostname.\n",optarg);
                return 2;
            }
            if(tmp==optarg+strlen(optarg)-1)
            {
                fprintf(stderr,"Error in option --proxy %s Port number is missing.\n",optarg);
                return 2;
            }
            *tmp='\0';//将“:”替换为“\0”，切割ip和port
            proxyport=atoi(tmp+1);break;//获取port
            case ':':
            case 'h':
            case '?': usage();return 2;break;
            case 'c': clients=atoi(optarg);break;//获取需要创建的客户端个数
        }
    }
    //如果optind == argc，说明缺少url参数
    if(optind==argc) {
        fprintf(stderr,"webbench: Missing URL!\n");
        usage();
        return 2;
    }

    if(clients==0) clients=1;//防止命令行中 -c 0从而出现bug
    if(benchtime==0) benchtime=30;//防止命令行中 -t 0从而出现bug
 
    /* Copyright */
    fprintf(stderr,"Webbench - Simple Web Benchmark "PROGRAM_VERSION"\n"
            "Copyright (c) Radim Kolar 1997-2004, GPL Open Source Software.\n"
            );
 
    build_request(argv[optind]);//将url传入build_request函数中
 
    // print request info ,do it in function build_request
    /*printf("Benchmarking: ");
 
    switch(method)
    {
        case METHOD_GET:
        default:
        printf("GET");break;
        case METHOD_OPTIONS:
        printf("OPTIONS");break;
        case METHOD_HEAD:
        printf("HEAD");break;
        case METHOD_TRACE:
        printf("TRACE");break;
    }
    
    printf(" %s",argv[optind]);
    
    switch(http10)
    {
        case 0: printf(" (using HTTP/0.9)");break;
        case 2: printf(" (using HTTP/1.1)");break;
    }
 
    printf("\n");
    */

    printf("Runing info: ");

    if(clients==1) 
        printf("1 client");
    else
        printf("%d clients",clients);

    printf(", running %d sec", benchtime);
    
    if(force) printf(", early socket close");
    if(proxyhost!=NULL) printf(", via proxy server %s:%d",proxyhost,proxyport);
    if(force_reload) printf(", forcing reload");
    
    printf(".\n");
    
    return bench();//进入这个函数
}

//创建请求报文，存到request数组中
void build_request(const char *url)
{
    char tmp[10];
    int i;

    //bzero(host,MAXHOSTNAMELEN);
    //bzero(request,REQUEST_SIZE);
    memset(host,0,MAXHOSTNAMELEN);
    memset(request,0,REQUEST_SIZE);
    //由于http1.0或者1.2才支持一些特性，在此重新设置http协议头
    if(force_reload && proxyhost!=NULL && http10<1) http10=1;
    if(method==METHOD_HEAD && http10<1) http10=1;
    if(method==METHOD_OPTIONS && http10<2) http10=2;
    if(method==METHOD_TRACE && http10<2) http10=2;
    //根据请求头进行跳转,封装后的报文类似：
    /*
    GET / HTTP/1.0
    User-Agent: WebBench 1.5
    Host: gls.show
    */
    switch(method)
    {
        default:
        case METHOD_GET: strcpy(request,"GET");break;
        case METHOD_HEAD: strcpy(request,"HEAD");break;
        case METHOD_OPTIONS: strcpy(request,"OPTIONS");break;
        case METHOD_TRACE: strcpy(request,"TRACE");break;
    }

    strcat(request," ");//strstr进行字符串的拼接

    if(NULL==strstr(url,"://"))//检查url中是否有://字符串
    {
        fprintf(stderr, "\n%s: is not a valid URL.\n",url);
        exit(2);
    }
    if(strlen(url)>1500)
    {
        fprintf(stderr,"URL is too long.\n");
        exit(2);
    }
    if (0!=strncasecmp("http://",url,7)) 
    { 
        fprintf(stderr,"\nOnly HTTP protocol is directly supported, set --proxy for others.\n");
        exit(2);
    }
    
    /* protocol/host delimiter */
    i=strstr(url,"://")-url+3;

    if(strchr(url+i,'/')==NULL) {
        fprintf(stderr,"\nInvalid URL syntax - hostname don't ends with '/'.\n");
        exit(2);
    }
    
    if(proxyhost==NULL)//未设置代理服务器
    {
        /* get port from hostname */
        if(index(url+i,':')!=NULL && index(url+i,':')<index(url+i,'/'))
        {
            strncpy(host,url+i,strchr(url+i,':')-url-i);
            //bzero(tmp,10);
            memset(tmp,0,10);
            strncpy(tmp,index(url+i,':')+1,strchr(url+i,'/')-index(url+i,':')-1);
            /* printf("tmp=%s\n",tmp); */
            proxyport=atoi(tmp);//解析出port
            if(proxyport==0) proxyport=80;
        } 
        else
        {
            strncpy(host,url+i,strcspn(url+i,"/"));
        }
        // printf("Host=%s\n",host);
        strcat(request+strlen(request),url+i+strcspn(url+i,"/"));
    } 
    else
    {
        // printf("ProxyHost=%s\nProxyPort=%d\n",proxyhost,proxyport);
        strcat(request,url);
    }

    if(http10==1)
        strcat(request," HTTP/1.0");
    else if (http10==2)
        strcat(request," HTTP/1.1");
  
    strcat(request,"\r\n");//CRLF
  
    if(http10>0)
        strcat(request,"User-Agent: WebBench "PROGRAM_VERSION"\r\n");
    if(proxyhost==NULL && http10>0)
    {
        strcat(request,"Host: ");
        strcat(request,host);
        strcat(request,"\r\n");
    }
 
    if(force_reload && proxyhost!=NULL)
    {
        strcat(request,"Pragma: no-cache\r\n");
    }
  
    if(http10>1)
        strcat(request,"Connection: close\r\n");
    
    /* add empty line at end */
    if(http10>0) strcat(request,"\r\n"); 
    
    printf("\nRequest:\n%s\n",request);
}

/* vraci system rc error kod */
static int bench(void)
{
    int i,j,k;	
    pid_t pid=0;
    FILE *f;

    /* check avaibility of target server */
    i=Socket(proxyhost==NULL?host:proxyhost,proxyport);//socket函数在socket.c中已经写好
    if(i<0) { 
        fprintf(stderr,"\nConnect to server failed. Aborting benchmark.\n");
        return 1;
    }
    close(i);
    
    /* create pipe */
    if(pipe(mypipe))//创建管道+错误处理
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
    //fork出进程，注意子进程由于break无法继续fork
    /* fork childs */
    for(i=0;i<clients;i++)
    {
        pid=fork();
        if(pid <= (pid_t) 0)
        {
            /* child process or error*/
            sleep(1); /* make childs faster */
            break;//子进程直接跳出循环，从而fork出指定数量的进程，而非指数型产生进程
        }
    }

    if( pid < (pid_t) 0)
    {
        fprintf(stderr,"problems forking worker no. %d\n",i);
        perror("fork failed.");
        return 3;
    }

    if(pid == (pid_t) 0)//子进程把数据通过管道传给父进程
    {
        /* I am a child */
        if(proxyhost==NULL)
            benchcore(host,proxyport,request);//调用benchcore
        else
            benchcore(proxyhost,proxyport,request);

        /* write results to pipe */
        f=fdopen(mypipe[1],"w");//子进程进行写操作
        if(f==NULL)
        {
            perror("open pipe for writing failed.");
            return 3;
        }
        /* fprintf(stderr,"Child - %d %d\n",speed,failed); */
        fprintf(f,"%d %d %d\n",speed,failed,bytes);//把speed、failed、bytes写到f
        fclose(f);

        return 0;
    } 
    else//父进程操作
    {
        f=fdopen(mypipe[0],"r");//读取子进程写入的数据
        if(f==NULL) 
        {
            perror("open pipe for reading failed.");
            return 3;
        }
        
        setvbuf(f,NULL,_IONBF,0);
        
        speed=0;
        failed=0;
        bytes=0;
    
        while(1)
        {
            pid=fscanf(f,"%d %d %d",&i,&j,&k);//把从f中读取的数据格式化到i j k，这三个分别对应speed、failed、bytes,fscanf为阻塞式函数
            if(pid<2)//pid是fscanf函数的返回个数
            {
                fprintf(stderr,"Some of our childrens died.\n");
                break;
            }
            
            speed+=i;
            failed+=j;
            bytes+=k;
        
            /* fprintf(stderr,"*Knock* %d %d read=%d\n",speed,failed,pid); */
            if(--clients==0) break;
        }
    
        fclose(f);
        //输出最终的数据结果
        printf("\nSpeed=%d pages/min, %d bytes/sec.\nRequests: %d susceed, %d failed.\n",
            (int)((speed+failed)/(benchtime/60.0f)),//一分钟内成功\失败的次数
            (int)(bytes/(float)benchtime),//每秒传输的字节数
            speed,//speed就是succeed的次数
            failed);//失败次数
    }
    
    return i;
}

void benchcore(const char *host,const int port,const char *req)
{
    int rlen;
    char buf[1500];
    int s,i;
    struct sigaction sa;

    /* setup alarm signal handler */
    sa.sa_handler=alarm_handler;
    sa.sa_flags=0;
    if(sigaction(SIGALRM,&sa,NULL))//给信号绑定函数
        exit(3);
    
    alarm(benchtime); // after benchtime,then exit  在这里开始计时

    rlen=strlen(req);//包的长度
    nexttry:while(1)//在收到信号前无限循环
    {
        if(timerexpired)//每次循环都测试是否到时间
        {
            if(failed>0)
            {
                /* fprintf(stderr,"Correcting failed by signal\n"); */
                failed--;
            }
            return;
        }
        //failed：socket建立失败、写数据失败、读取数据失败
        s=Socket(host,port);    
        if(s<0) { failed++;continue;} 
        if(rlen!=write(s,req,rlen)) {failed++;close(s);continue;}
        if(http10==0) 
        if(shutdown(s,1)) { failed++;close(s);continue;}
        if(force==0) //接受数据，force默认为0
        {
            /* read all available data from socket */
            while(1)
            {
                if(timerexpired) break; 
                i=read(s,buf,1500);
                /* fprintf(stderr,"%d\n",i); */
                if(i<0) //读取数据失败
                { 
                    failed++;
                    close(s);
                    goto nexttry;//重新进入最外层while循环
                }
                else
                if(i==0) break;
                else
                bytes+=i;//接受到的数据
            }
        }
        if(close(s)) {failed++;continue;}
        speed++;//如果能执行至此，speed+1
    }
}
