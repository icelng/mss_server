#include "comunication.h"
#include "stdlib.h"
#include "stdio.h"
#include "arpa/inet.h"  //struct in_addr
#include "sys/socket.h"  //使用套接字
#include "sys/syslog.h"
#include "sys/time.h"
#include "signal.h"
#include "errno.h"   //错误号定义
#include "semaphore.h"  //使用信号量
#include "list.h"   //使用链表
#include "string.h"
#include "fcntl.h"
//#include "mysql.h"

/* 函数名: int com_recv_str(int sockfd,char *str,int size)
 * 功能: 从套接字中接收一个字符串,以socket流中的/n/r为分割符号
 * 参数: int sockfd,接收字符串的套接字
 *       char *str,接收字符串的存储空间
 *       int size,缓冲区的最大空间
 *       int timeout_mode,是否允许阻塞
 * 返回值: -1,接收失败
 *         >0,接收到的字符串的长度
 */
int com_recv_str(int sockfd,char *str,int size,int timeout_mode){
    struct timeval timeout;   //设置超时用
    char recv_c;
    int n = 0;   //记录接收的字符数,超过size则报错退出
    if(timeout_mode == 1){
        timeout.tv_sec = RECV_TIMEOUT_SEC;
        timeout.tv_usec = 0;
    }else{
        timeout.tv_sec = 0;
        timeout.tv_usec = 0;
    }
    //设置接收超时
    setsockopt(sockfd,SOL_SOCKET,SO_RCVTIMEO,(char*)&timeout,sizeof(struct timeval));
    while(1){
        if(recv(sockfd,&recv_c,1,0) <= 0){
            syslog(LOG_DEBUG,"recv string failed:%s",strerror(errno));
            return -1;
        }
        if(recv_c == '\n'){  //换行符表示结束
            str[n] = 0;
            return n;
        }else if(recv_c == '\r'){
            n = 0;
        }else{  
            if(recv_c == 0x10){  //为转义符号,说明下一个字符为数据，不是控制字符
                if(recv(sockfd,&recv_c,1,0) <= 0){
                    syslog(LOG_DEBUG,"comunication recv str failed:%s",strerror(errno));
                    return -1;
                }
                if(n >= size - 1){
                    return -2;
                }
                str[n++] = recv_c;
            }else{
                if(n >= size - 1){
                    return -2;
                }
                str[n++] = recv_c;
            }
        }
    }
}

/* 函数名: int com_send_str(int sockfd,char *send_str,int size)
 * 功能: 通过套接字发送字符串
 * 参数: int sockfd,发送字符串的套接字
 *       char *send_str,需要发送的字符串的指针
 *       int size,字符串的长度
 * 返回值: -1,
 *          1,
 */
int com_send_str(int sockfd,char *send_str,int str_len){
    int index = 0;
    int buf_size = 0;
    char send_c;
    char send_buf[1024];


    while(send_str[index] != 0 && index < str_len){
        if(buf_size >= 1023){  //分段发送
            if(send(sockfd,send_buf,buf_size,0) < 0){
                syslog(LOG_DEBUG,"com_send_str error:%s",strerror(errno));
                return -1;
            }
            buf_size = 0;
        }
        send_c = send_str[index++];
        //控制字符要加上链路转义字符，以实现透明传输
        if(send_c == 0x10 || send_c == '\r' || send_c == '\n'){
            send_buf[buf_size++] = 0x10;
            send_buf[buf_size++] = send_c;
        }else{
            send_buf[buf_size++] = send_c;
        }
    }
    if(buf_size >= 1023){  //分段发送
        if(send(sockfd,send_buf,buf_size,0) < 0){
            syslog(LOG_DEBUG,"com_send_str error:%s",strerror(errno));
            return -1;
        }
        buf_size = 0;
    }
    send_buf[buf_size++] = '\n';
    send_buf[buf_size++] = '\r';
    if(send(sockfd,send_buf,buf_size,0) < 0){
        syslog(LOG_DEBUG,"com_send_str error:%s",strerror(errno));
        return -1;
    }
    return 1;
}
