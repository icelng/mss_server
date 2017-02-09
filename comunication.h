#include "list.h"   //使用链表
#include "semaphore.h"  //使用信号量

#define MAX_RECV_STR_LENGTH 4096
#define RECV_TIMEOUT_SEC 10

struct msgq_node{
    struct list_head list;
    unsigned int data_size;
    void *data;
};

struct msgq_s{
    struct list_head head; //链表头
    struct mm_pool_s *mmpl;  //所使用的内存池
    sem_t mutex;  //链表互斥锁
    sem_t msgq_l;  //队列长度,长度为0的时候获取队列元素会阻塞
};


int com_recv_str(int socfd,char *str,int size,int timeout_mode);
int com_send_data(int sockfd,char *send_str,int data_size);
int com_rsa_send(int sockfd,char *pub_key,char *send_buf);
int com_rsa_recv(int sockfd,char *prvi_key,char *recv_buf,int buf_size,int timeout_mode);
int com_rsa_send_aeskey(int sockfd,char *pub_key,unsigned char *aes_key,int n_bits);
int com_pipe_wr_data(int wrfd,char *wr_buf,int wr_size);
int com_pipe_rd_data(int rdfd,char *rd_buf,int buf_size);
int com_transparent(unsigned char *data_src,unsigned char *data_dst,int *data_size,int buf_size);
int com_remove_ctlsymbols(unsigned char *src,unsigned char *dst,int buf_size);
int com_msgq_create(struct msgq_s **p_new_msgq);
int com_msgq_snd(struct msgq_s *p_msgq,void *msg_buf,unsigned int msg_size);
int com_msgq_recv(struct msgq_s *p_msgq,void *data);
int com_rcv_data(int sockfd,char *rcv_buf,int buf_size);
int com_snd_data(int sockfd,char *snd_data,int data_size,int enc_flg);
