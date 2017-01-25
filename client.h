#include "arpa/inet.h"
#include "semaphore.h"  //使用信号量
#include "list.h"
#include "encdec.h"

#define CLIENT_NAME_LENGTH 32
#define MAX_PASSWD_LENGTH 32
#define MAX_QUERY_STR_LENGTH 128
#define WD_RESUME_CNT 30
#define VERIFY_TIMEOUT 5
#define CLIENT_AES_KEY_LENGTH 128
#define CLIENT_MAX_EPOLL_EVENTS 64
#define CLIENT_RECV_BUF_SIZE 4096
#define CLIENT_SEND_BUF_SIZE 4096
#define CLIENT_MAX_MSG_DATA_SIZE 4096
#define CLIENT_MSGTEXT_LENGTH 4
#define CLIENT_DEC_MSQ_KEY 666
#define CLIENT_INFOTYPE_L 32
#define CLIENT_INFOCONTENT_L 4064


struct cm_msg{   //通信报文,cm即是comunication
    unsigned int client_id; //客户端id
    unsigned short type;
    unsigned short req_er_type;  //请求类型或者错误类型
    unsigned int msg_cnt;
    unsigned int data_size;
    unsigned char data[CLIENT_MAX_MSG_DATA_SIZE];
};

/*发送队列*/
struct snd_queue{
    struct list_head queue;  //使用双向链表来作队列
    unsigned int snd_size;
    unsigned int snd_start;  //数据发送起始指针,用在一次发送请求无法发送完数据后再次请求发送
    unsigned char *snd_data;
};

struct client_info{
    struct list_head list;  //链表
    struct in_addr ip;   //客户端IP
    char name[CLIENT_NAME_LENGTH];  //客户端名称
    int id;  //客户端的ID
    int lv;   //客户端级别(0,1,2,0是最高级,可使用所有的指令),级别决定各种权限
    int sockfd;  //客户端套接字
    int wd_cnt;   //看门狗，，如果计时到0，则说明已经失去链接,应该删去该客户端
    int queote_cnt; //客户端信息被引用次数,只有引用次数为0的时候，才释放删除使能锁
    sem_t queote_cnt_mutex;  //互斥访问客户端信息结构体的引用次数，引用次数大于零的时候，会占用删除使能锁
    sem_t del_enable;   //删除使能锁，，只有在拿到这个锁的时候，才能够删除该结构体
    int msg_cnt;  //报文计数
    char recv_buf[CLIENT_RECV_BUF_SIZE]; //保存着接收到的一个数据报密文的缓冲区,即将解密
    int recv_size;
    int recv_ready;
    int recv_is_datachar;  //透明传输所用
    struct list_head sndq_head; //发送队列头
    sem_t sndq_mutex; //发送队列锁
    char pub_key[1024];  //客户端RSA公钥
    char priv_key[1024];  //对应于该客户端的服务器RSA私钥
    AES_KEY aes_enc_key;   //AES加密秘钥
    AES_KEY aes_dec_key;   //AES解密秘钥
};

/* 函数:
 * 功能:
 * 参数:
 * 返回值:
 */
int client_manage_init();  //初始化客户端管理
int client_add(char name[],int id,int ip,int sockfd);  //添加客户端
int client_remove(int client_id);     //根据客户端ID移除客户端
//int __client_del(struct client_info *p_client_i);  //删除客户端,内部调用,没有加锁
struct client_info* __client_search(int client_id);   //根据客户端ID查找,访问列表没有没有加锁
struct client_info* client_search(int client_id);   //根据客户端ID查找,有加锁
int client_wd_init();   //看门狗初始化
int client_wd_resume(int client_id);
void *client_create_thread(void *);
int client_create(int sockfd,struct sockaddr_in);
int client_recv_str(int sockfd,char *,int);
int client_mysql_connect();
void client_wd_decline();   //看门狗计时器衰减,如果发现有计数到零的客户端，则删去
void *client_recv_thread();
void *client_dec_parse_thread();
void *client_enc_thread();
void *client_snd_thread();
void *client_snd_test_thread();  //调试所需要的测试线程
struct client_info* client_get_ci(int client_id);
int client_release_ci(struct client_info *p_c_i);
int client_msq_init();
int client_parse_do(char *info_src,struct client_info *p_c_i);
//int client_verify(int sockfd,struct in_addr,struct verify_info); //客户端验证，验证成功返回1否则返回小于0的数

