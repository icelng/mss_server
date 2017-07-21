#include "stdlib.h"
#include "stdio.h"
#include "arpa/inet.h"  //struct in_addr
#include "sys/socket.h"  //使用套接字
#include "sys/syslog.h"
#include "sys/time.h"
#include "signal.h"
#include "errno.h"   //错误号定义
#include "string.h"
#include "fcntl.h"
#include "encdec.h"  //需要加密传输
#include "comunication.h"
#include "unistd.h"
#include "mmpool.h"  //还是用了内存池
//#include "mysql.h"



/* 函数名: int com_remove_ctlsymbols(unsigned char *src,unsigned char *dst,int buf_size)
 * 功能: 除去控制符号
 * 参数: unsigned char *src,原数据,必须是以符号\n结束
 *       unsigned char *dst,保存处理后数据的缓存
 *       int buf_size,缓存大小
 * 返回值: -1,
 *          1,
 */
int com_remove_ctlsymbols(unsigned char *src,unsigned char *dst,int buf_size){
    int  src_i,dst_i;
    char c_tmp;
    
    src_i = 0;
    dst_i = 0;
    while(1){
        if(dst_i >= buf_size)return -1;  //如果超出了保存处理后数据的缓存
        c_tmp = src[src_i++];
        if(c_tmp == '\r'){
            dst_i = 0;
            continue;
        }else if(c_tmp == '\n'){
            return dst_i;
        }else if(c_tmp == 0x10){
            dst[dst_i++] = src[src_i++];
            continue;
        }
        dst[dst_i++] = c_tmp;
    }
}

/* 函数名: int com_transparent(unsigned char *data_src,unsigned char *data_dst,int *size)
 * 功能: 数据透明化，指的是加入转义符使得能够透明传输
 * 参数: unsigned char *data_src,原数据
 *       unsigned char *data_dst,处理后的数据
 *       int *data_size,作为输入时，是原数据的大小；作为输出时是处理后数据的大小
 *       int buf_size,保存处理好的数据的缓冲区的大小
 * 返回值: -1
 *          1
 */
int com_transparent(unsigned char *data_src,unsigned char *data_dst,int *data_size,int buf_size){
    int src_index = 0;
    int dst_index = 0;
    int src_size;
    unsigned char c_tmp;

    src_size = *data_size;
    data_dst[dst_index++] = '\r';
    (*data_size)++;
    while(src_index < src_size){
        if(dst_index >= buf_size)return -1;
        c_tmp = data_src[src_index++];
        if(c_tmp == '\r' || c_tmp == '\n' || c_tmp == 0x10){
            data_dst[dst_index++] = 0x10;
            (*data_size)++;
        }
        data_dst[dst_index++] = c_tmp;
    }
    if(dst_index + 1 > buf_size)return -1;
    data_dst[dst_index++] = '\n';
    (*data_size) += 1;
    //syslog(LOG_DEBUG,"src_size:%d,dst_size:%d",src_size,*data_size); //测试用的代码，可删去
    return 1;
}


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

/* 函数名: int com_snd_data(int sockfd,char *snd_data,int data_size,int enc_flg)
 * 功能: 通过套接字给对方发送数据
 * 参数: int sockfd,套接字
 *       char *snd_data,需要发送的数据
 *       int data_size,发送数据大小
 *       int enc_flg,发送的数据是否已经加密
 * 返回值: -1,
 *          1,
 */
int com_snd_data(int sockfd,char *snd_data,int data_size,int enc_flg){
    unsigned short snd_size;
    unsigned short real_snd_size;
    union{
        unsigned short u16;
        char u8[2];
    }head;
    const char syc_char = '\r'; //同步符号
    const char ctl_char = 0x10;
    int i;
    snd_size = data_size;
    head.u16 = (snd_size << 1) | enc_flg; //不加密
    for(i = 0;i < 5;i++){
        send(sockfd,&syc_char,1,0);  //发送3个同步符号
    }
    for(i = 0;i < 2;i++){
        if(head.u8[i] == syc_char || head.u8[i] == ctl_char){
            send(sockfd,&ctl_char,1,0);
            send(sockfd,&head.u8[i],1,0);
        }else{
            send(sockfd,&head.u8[i],1,0);
        }
    }
    real_snd_size = send(sockfd,snd_data,snd_size,0); //发送报文
    return real_snd_size;
}

/* 函数名: int com_rcv_data(int sockfd,char *rcv_buf,int buf_size)
 * 功能: 从套接字上接收一个数据包
 * 参数: int sockfd,套接字描述符
 *       char *rcv_buf,接收数据的缓存
 *       int buf_size,缓存大小
 * 返回值: -1,接收出现了错误
 *        >=0,实际接收大小
 */
int com_rcv_data(int sockfd,char *rcv_buf,int buf_size){
    struct timeval timeout;   //设置超时用
    unsigned short rcv_size = 0;
    int syc_cnt = 0;  //同步计数
    int monitor_cnt = 0; //监听字符计数
    int h_i = 0; //接收head的index
    union{
        unsigned short u16;
        unsigned char u8[2];
    }head;
    const char syc_c = '\r';  //同步字符
    const char ctl_c = 0x10;  //控制字符
    char rcv_c;
    int rcv_status = 0;//0,处于监听状态，1处于报文接收状态

    timeout.tv_sec = RECV_TIMEOUT_SEC;
    timeout.tv_usec = 0;
    //设置接收超时
    setsockopt(sockfd,SOL_SOCKET,SO_RCVTIMEO,(char*)&timeout,sizeof(struct timeval));

    while(1){
        if(rcv_status == 0){  //处于报文接收状态
            if(monitor_cnt++ == 50){ //如果监听的字符计数超过了50则退出
                return -1;
            }
            if(recv(sockfd,&rcv_c,1,0) == -1){
                return -1;
            }
            if(rcv_c == syc_c){
                if(++syc_cnt == 3){  //开始接收head
                    while(1){
                        if(recv(sockfd,&rcv_c,1,0) == -1){
                            return -1;
                        }
                        if(rcv_c == syc_c)continue;
                        if(rcv_c == ctl_c){
                            if(recv(sockfd,&rcv_c,1,0) == -1){
                                return -1;
                            }
                        }
                        head.u8[h_i++] = rcv_c;
                        if(h_i == 2){
                            rcv_status = 1;
                            rcv_size = head.u16 << 1;
                            if(rcv_size > buf_size)return -1;
                            break;
                        }
                    }
                }
            }else{
                syc_cnt = 0;
            }
        }else if(rcv_status == 1){
            if(recv(sockfd,rcv_buf,rcv_size,0) == -1){
                return -1;
            }
            break;
        }
    }
    return 1;
}

/* 函数名: int com_send_data(int sockfd,char *send_str,int size)
 * 功能: 通过套接字发送字符串
 * 参数: int sockfd,发送字符串的套接字
 *       char *send_str,需要发送的字符串的指针
 *       int size,字符串的长度
 * 返回值: -1,
 *          1,
 */
int com_send_data(int sockfd,char *send_str,int data_size){
    int index = 0;
    int buf_size = 0;
    int send_size = 0;
    int real_snd_size = 0;
    char send_c;
    char send_buf[1024];


    while(index < data_size){
        if(buf_size >= 1023){  //分段发送
            if((real_snd_size = send(sockfd,send_buf,buf_size,0)) < 0){
                if(errno == EWOULDBLOCK)return send_size;//如果发送缓冲区已经满了,则返回
                syslog(LOG_DEBUG,"com_send_str error:%s",strerror(errno));
                return -1;
            }
            buf_size = 0;
        }
        send_c = send_str[index++];
        send_size++;
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
            if(errno == EWOULDBLOCK)return send_size;//如果发送缓冲区已经满了,则返回
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
    com_snd_data(sockfd,send_cipher,strlen(send_cipher) + 1,1);
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
        n = 16;
    }else if(n_bits == 192){
        n = 24;
    }else if(n_bits == 256){
        n = 32;
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


    send_buf[buf_size++] = '\r';
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
    if(write(wrfd,send_buf,buf_size) < 0){
        syslog(LOG_DEBUG,"com_pipe_wr_data() error:%s",strerror(errno));
        return -1;
    }
    return 1;
}



/* 函数名: int com_msgq_create(struct msgq_s **p_new_msgq)
 * 功能: 创建新的消息队列,因为消息队列的创建并不频繁
 * 参数: struct msgq_s **p_new_msgq,消息队列的指针的指针
 * 返回值: -1, 
 *          1,
 */
int com_msgq_create(struct msgq_s **p_new_msgq){
    struct mm_pool_s *mmpl = NULL;
    if(mmpl_create(&mmpl) == -1)return -1; //创建内存池，一个消息队列使用一个内存池
    *p_new_msgq = (struct msgq_s*)mmpl_getmem(mmpl,sizeof(struct msgq_s));
    if(p_new_msgq == NULL)return -1;
    INIT_LIST_HEAD(&(*p_new_msgq)->head);
    (*p_new_msgq)->mmpl = mmpl; //依附内存池
    sem_init(&(*p_new_msgq)->mutex,0,1);  //初始化链表互斥锁
    sem_init(&(*p_new_msgq)->msgq_l,0,0); //初始化队列长度的信号量
    return 1;
}


/* 函数名: int com_msgq_destroy(struct msgq_s *p_msgq)
 * 功能: 销毁消息队列
 * 参数: struct msg_s *p_msgq,消息队列结构指针
 * 返回值: -1,
 *          1,
 */
//int com_msgq_destroy(struct msgq_s *p_msgq){
//    struct msgq_node msgq_n;
//    return 1;
//}

/* 函数名: int com_msgq_snd(struct msgq_s *p_msgq,void *data,int msg_size)
 * 功能: 通过消息队列发送消息
 * 参数: struct msgq_s *p_msgq,消息队列结构体
 *       void *data,消息的首指针
 * 返回值: -1,
 *          1
 */
int com_msgq_snd(struct msgq_s *p_msgq,void *msg_buf,unsigned int msg_size){
    struct msgq_node *p_msgq_n;
    void *data;

    p_msgq_n = mmpl_getmem(p_msgq->mmpl,sizeof(struct msgq_node));
    //p_msgq_n = malloc(sizeof(struct msgq_node));
    if(p_msgq_n == NULL)return -1;
    data = mmpl_getmem(p_msgq->mmpl,msg_size);
    //data = malloc(msg_size);
    if(data == NULL){
        mmpl_rlsmem(p_msgq->mmpl,p_msgq_n);
        //free(p_msgq_n);
        return -1;
    }
    memcpy(data,msg_buf,msg_size);
    p_msgq_n->data = data;
    p_msgq_n->data_size = msg_size;
    sem_wait(&p_msgq->mutex);  //互斥访问链表
    list_add_tail(&p_msgq_n->list,&p_msgq->head);//链入队尾
    sem_post(&p_msgq->mutex);
    sem_post(&p_msgq->msgq_l);
    return 1;
}


/* 函数名: int com_msgq_recv(struct msgq_s p_msgq,void *data)
 * 功能: 从消息队列中获取到一条消息
 * 参数: struct msgq_s p_msgq,消息队列结构体的指针
 *       void *data;
 * 返回值: -1,
 *          1,
 */
int com_msgq_recv(struct msgq_s *p_msgq,void *data){
    struct msgq_node *p_msgq_n;
    sem_wait(&p_msgq->msgq_l);  //等待至消息队列有消息
    sem_wait(&p_msgq->mutex);  //互斥使用消息队列
    p_msgq_n = list_entry(p_msgq->head.next,struct msgq_node,list);
    list_del(p_msgq->head.next);
    sem_post(&p_msgq->mutex);  //互斥使用消息队列
    memcpy(data,p_msgq_n->data,p_msgq_n->data_size);
    mmpl_rlsmem(p_msgq->mmpl,p_msgq_n->data);
    mmpl_rlsmem(p_msgq->mmpl,p_msgq_n);
    //free(p_msgq_n->data);
    //free(p_msgq_n);
    return 1;
}

