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
#include "encdec.h"  //需要加密传输
#include "comunication.h"
#include "unistd.h"
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


/* 函数名: int com_rsa_recv()
 * 功能: 接收一个字符串，并且解密好
 * 参数: int sockfd,接收字符串的套接字
 *       char *prvi_key,私钥
 *       char *recv_buf,接收字符串的缓冲区
 *       int buf_size,缓冲区的大小
 *       int timeout_mode,超时模式，若为0则阻塞，1则超时接收
 * 返回值: -1,接收失败
 *         >0,接收到的字符串大小
 */       
int com_rsa_recv(int sockfd,char *prvi_key,char *recv_buf,int buf_size,int timeout_mode){
    //这里的plain和recv_cipher所需的存储空间大小要相同，不然执行rsa_priv_decrypt会报错的
    int ret;
    char recv_cipher[MAX_RECV_STR_LENGTH];
    char plain[MAX_RECV_STR_LENGTH];  
    ret = com_recv_str(sockfd,recv_cipher,buf_size,timeout_mode);  
    if(ret < 0)return ret;
    ret = rsa_priv_decrypt(prvi_key,recv_cipher,plain);  //解密
    if(ret < 0)return ret;
    strncpy(recv_buf,plain,buf_size);
    return strlen(recv_buf);
}

/* 函数名: int com_rsa_send(int sockfd,char *pub_key,char *send_buf,int buf_size)
 * 功能: 发送一串加密的字符串
 * 参数:
 * 返回值:
 */
int com_rsa_send(int sockfd,char *pub_key,char *send_buf){
    int ret;
    char send_cipher[MAX_RECV_STR_LENGTH];
    
    ret = rsa_pub_encrypt(pub_key,send_buf,send_cipher);
    if(ret < 0)return ret;
    com_send_str(sockfd,send_cipher,strlen(send_cipher));
    if(ret < 0)return ret;
    return 1;
}

/* 函数名: int com_send_aeskey(int sockfd,unsigned char *aes_key,int n_bits)
 * 功能: 发送aes_key
 * 参数:
 * 返回值:
 */
int com_rsa_send_aeskey(int sockfd,char *pub_key,unsigned char *aes_key,int n_bits){
    char send_buf[257];
    int n = 0;
    int i;
    if(n_bits == 128){
        n = 128;
    }else if(n_bits == 192){
        n = 192;
    }else if(n_bits == 256){
        n = 256;
    }else{
        return -1;
    }
    for(i = 0;i < n;i++){
        send_buf[i] = aes_key[i];
    }
    send_buf[i] = 0;
    if(com_rsa_send(sockfd,pub_key,send_buf) < 0){
        return -2;
    }
    return 1;
}


/* 函数名: int com_pipe_rd_data(int rdfd,char *rd_buf,int buf_size)
 * 功能: 从管道中读取数据,以\n为分割
 * 参数: int rdfd,管道描述符
 *       char *rd_buf,读取缓存
 *       int buf_size,缓存大小
 * 返回值: -1,读取出错
 *         >0,实际读取大小
 */
int com_pipe_rd_data(int rdfd,char *rd_buf,int buf_size){
    char recv_c;
    int n = 0;   //记录接收的字符数,超过size则报错退出
    //设置接收超时
    while(1){
        if(read(rdfd,&recv_c,1) <= 0){
            syslog(LOG_DEBUG,"recv string failed:%s",strerror(errno));
            return -1;
        }
        if(recv_c == '\n'){  //换行符表示结束
            //rd_buf[n] = 0;
            return n;
        }else if(recv_c == '\r'){
            n = 0;
        }else{  
            if(recv_c == 0x10){  //为转义符号,说明下一个字符为数据，不是控制字符
                if(read(rdfd,&recv_c,1) <= 0){
                    syslog(LOG_DEBUG,"comunication recv str failed:%s",strerror(errno));
                    return -1;
                }
                if(n >= buf_size){
                    return -1;
                }
                rd_buf[n++] = recv_c;
            }else{
                if(n >= buf_size){
                    return -1;
                }
                rd_buf[n++] = recv_c;
            }
        }
    }
    return n;
}


/* 函数名: int com_pipe_wr_data(int wrfd,char *wr_buf,int wr_size)
 * 功能: 往管道写数据
 * 参数: int wrfd,写管道描述符
 *       char *wr_buf,保存需要发送的数据的缓冲区
 *       int wr_size,发送数据的大小,注意,大小不能超过发送缓冲区
 * 返回值: -1,失败
 *          1,成功
 */
int com_pipe_wr_data(int wrfd,char *wr_buf,int wr_size){
    int index = 0;
    int buf_size = 0;
    char send_c;
    char send_buf[1024];


    while(index < wr_size){
        if(buf_size >= 1023){  //分段发送
            if(write(wrfd,send_buf,buf_size) < 0){
                syslog(LOG_DEBUG,"com_pipe_wr_data()error:%s",strerror(errno));
                return -1;
            }
            buf_size = 0;
        }
        send_c = wr_buf[index++];
        //控制字符要加上链路转义字符，以实现透明传输
        if(send_c == 0x10 || send_c == '\r' || send_c == '\n'){
            send_buf[buf_size++] = 0x10;
            send_buf[buf_size++] = send_c;
        }else{
            send_buf[buf_size++] = send_c;
        }
    }
    if(buf_size >= 1023){  //分段发送
        if(write(wrfd,send_buf,buf_size) < 0){
            syslog(LOG_DEBUG,"com_pipe_wr_data()error:%s",strerror(errno));
            return -1;
        }
        buf_size = 0;
    }
    send_buf[buf_size++] = '\n';
    send_buf[buf_size++] = '\r';
    if(write(wrfd,send_buf,buf_size) < 0){
        syslog(LOG_DEBUG,"com_pipe_wr_data() error:%s",strerror(errno));
        return -1;
    }
    return 1;
}



