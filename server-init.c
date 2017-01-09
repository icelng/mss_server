#include "stdlib.h"
#include "stdio.h"
#include "string.h"
#include "sys/syslog.h"
#include "sys/socket.h"
#include "netinet/in.h"
#include "arpa/inet.h"
#include "fcntl.h"
#include "errno.h"  //错误号定义
#include "unistd.h"  //sleep()
#include "server-init.h"

struct sockaddr_in server_addr;

/* 函数: int server_socket_create(int *p_server_sockfd,int port)
 * 功能: 创建server的套接字
 * 参数: int *p_server_sockfd,指向套接字的指针
 *       int port,套接字对应的端口
 * 返回值: -1,
 *          1,
 */
int server_socket_create(int *p_server_sockfd,int port){
    int no = 1;


    if((*p_server_sockfd = socket(AF_INET,SOCK_STREAM,0)) == -1){
        syslog(LOG_DEBUG,"create server socket error:%s",strerror(errno));
        return -1;
    }
    if(setsockopt(*p_server_sockfd,SOL_SOCKET,SO_REUSEADDR,&no,sizeof(no)) < 0){  //设置允许端口重用
        syslog(LOG_DEBUG,"setsockopt error:%s",strerror(errno));
    }
    memset(&server_addr,0,sizeof(struct sockaddr_in));
    server_addr.sin_family = AF_INET;
    server_addr.sin_addr.s_addr = INADDR_ANY;
    server_addr.sin_port = htons(port);
    if(bind(*p_server_sockfd,(struct sockaddr*)&server_addr,sizeof(struct sockaddr_in)) == -1){
        syslog(LOG_DEBUG,"bind socket error:%s",strerror(errno));
        return -2;
    }
    syslog(LOG_DEBUG,"creat server socket successfully");
    return 1;
}

/* 函数: int log_init();
 * 功能: 设置syslog,把syslog输出到指定的文件
 * 参数: 
 * 返回值: -1,
 *          1,
 */
int log_init(){
    int logfd = open("debug.log",O_RDWR|O_CREAT|O_APPEND,0644);

    if(logfd == -1){
        return -1;
    }
    close(STDERR_FILENO);
    dup2(logfd,STDERR_FILENO);
    close(logfd);
    openlog("muma-server",LOG_PID|LOG_CONS|LOG_PERROR,0);   
    return 1;
}
