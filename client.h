#include "arpa/inet.h"
#include "list.h"

#define CLIENT_NAME_LENGTH 32
#define MAX_PASSWD_LENGTH 32
#define MAX_QUERY_STR_LENGTH 128
#define WD_RESUME_CNT 10
#define VERIFY_TIMEOUT 5


struct client_info{
    struct list_head list;  //链表
    struct in_addr ip;   //客户端IP
    char name[CLIENT_NAME_LENGTH];  //客户端名称
    int id;  //客户端的ID
    int lv;   //客户端级别(0,1,2,0是最高级,可使用所有的指令),级别决定各种权限
    int sockfd_cntl;  //客户端控制套接字,主要用来传输指令
    int wd_cnt;   //看门狗，，如果计时到0，则说明已经失去链接
    unsigned long tid_irecv_thread;   //指令接收线程ID
    char pub_key[4096];  //通信所用的公钥
    char priv_key[4096];  //私钥
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
void *client_create(void *);
int client_recv_str(int sockfd,char *,int);
int client_mysql_connect();
void client_wd_decline();   //看门狗计时器衰减,如果发现有计数到零的客户端，则删去
//int client_verify(int sockfd,struct in_addr,struct verify_info); //客户端验证，验证成功返回1否则返回小于0的数

