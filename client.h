#include "arpa/inet.h"

#define CLIENT_NAME_LENGTH 32
#define WD_RESUME_CNT 10
#define VERIFY_TIMEOUT 5
#define MAX_RECV_STR_LENGTH 64


struct verify_info{  //保存着验证信息
    char name[CLIENT_NAME_LENGTH];
    int id;
    int lv;
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
int client_create(int sockfd,struct in_addr);
int client_recv_str(int sockfd,char *,int);
void client_wd_decline();   //看门狗计时器衰减,如果发现有计数到零的客户端，则删去
//int client_verify(int sockfd,struct in_addr,struct verify_info); //客户端验证，验证成功返回1否则返回小于0的数

