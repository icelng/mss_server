#include "client.h"
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
#include "mysql.h"

struct client_info{
    struct list_head list;  //链表
    struct in_addr ip;   //客户端IP
    char name[CLIENT_NAME_LENGTH];  //客户端名称
    int id;  //客户端的ID
    int lv;   //客户端级别(0,1,2,0是最高级,可使用所有的指令),级别决定各种权限
    int sockfd_cntl;  //客户端控制套接字,主要用来传输指令
    int wd_cnt;   //看门狗，，如果计时到0，则说明已经失去链接
    unsigned long tid_irecv_thread;   //指令接收线程ID
};


sem_t client_list_mutex;  //互斥访问客户端列表
struct client_info client_i_h;   //链表头客户端

/* 函数: int client_manage_init()
 * 功能: 初始化客户端管理
 * 参数: 
 * 返回值: -1,
 *          1,
 */
int client_manage_init(){
    client_i_h.id = -1;  //链表头的客户端的ID是-1
    INIT_LIST_HEAD(&client_i_h.list);   //初始化客户端链表
    sem_init(&client_list_mutex,0,1);  //初始化信号量(锁)
    if (client_wd_init() == -1){   //初始化看门狗
        syslog(LOG_DEBUG,"client_manage_init-->client_wd_init error:return -1");
        return -1;
    }
}

/* 函数: int client_recv_str(int sockfd,char *str,int size)
 * 功能: 接收一个字符串，以socket流中的/n/r为分割符号
 * 参数: int sockfd,接受字符的socket
 *       char *str,接受字符串的缓冲区
 *       int size,缓冲区的最大空间
 * 返回值: -1,
 *          1,
 */
int client_recv_str(int sockfd,char *str,int size){
    struct timeval timeout = {3,0};   //设置超时用
    char recv_c;
    int n = 0;   //记录接收的字符数,超过size则报错退出
    //设置接收超时
    setsockopt(sockfd,SOL_SOCKET,SO_RCVTIMEO,(char*)&timeout,sizeof(struct timeval));
    while(1){
        if(recv(sockfd,&recv_c,1,0) <= 0){
            syslog(LOG_DEBUG,"verify failed:%s",strerror(errno));
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
                    syslog(LOG_DEBUG,"client recv str failed:%s",strerror(errno));
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
/* 函数: int client_verify(int sockfd_cntl,int ip,struct verify_info)
 * 功能: 验证客户端,验证成功后，创建客户端
 * 参数: int sockfd_cntl,客户端的控制套接字
 *       int ip,客户端的ip
 *       struct verify_info,返回的验证信息
 * 返回值: -1,通信出问题
 *         -2,不存在账户(名称)
 *         -3,账户密码错误
 *          1,验证成功
 */
int client_verify(int sockfd_cntl,struct in_addr ip,struct verify_info *p_v_i){
    char recv_str[MAX_RECV_STR_LENGTH];

    syslog(LOG_DEBUG,"ip:%s attempt to login",inet_ntoa(ip));
    if(client_recv_str(sockfd_cntl,recv_str,MAX_RECV_STR_LENGTH) < 0){
        return -1;
    }
    syslog(LOG_DEBUG,"client-verify name:%s",recv_str);
    strcpy(p_v_i->name,recv_str);
    if(client_recv_str(sockfd_cntl,recv_str,MAX_RECV_STR_LENGTH) < 0){
        return -1;
    }
    syslog(LOG_DEBUG,"client-verify id:%d",atoi(recv_str));
    p_v_i->id = atoi(recv_str);
    return 1;
}
/* 函数: int client_create()
 * 功能: 创建客户端,而且把新建的客户端链入链表
 * 参数: 
 * 返回值: -1,
 *          1,
 */
int client_create(int sockfd_cntl,struct in_addr ip){
    struct verify_info v_i;
    struct client_info *p_new_client_i;
    int err_ret;

    if((err_ret = client_verify(sockfd_cntl,ip,&v_i)) < 0){  //验证客户端
        syslog(LOG_DEBUG,"client_verify failed,faield number:%d",err_ret);  //验证失败，退出
        shutdown(sockfd_cntl,2);  //关闭套接字
        return err_ret;
    }
    p_new_client_i = (struct client_info*)malloc(sizeof(struct client_info));
    if(p_new_client_i == NULL)return -4;
    p_new_client_i->sockfd_cntl = sockfd_cntl;
    p_new_client_i->ip = ip;
    p_new_client_i->id = v_i.id;
    p_new_client_i->wd_cnt = WD_RESUME_CNT;
    strcpy(p_new_client_i->name,v_i.name);
    sem_wait(&client_list_mutex);  //互斥访问链表
    list_add(&p_new_client_i->list,&client_i_h.list);  //把新创建的客户端链入链表
    sem_post(&client_list_mutex);  //互斥访问链表
    syslog(LOG_DEBUG,"client:%s create complete!",p_new_client_i->name);
    return 1;
}

/* 函数: client_info* __client_search(int client_id)
 * 功能: 根据客户端ID查找客户端，访问链表没有加锁
 * 参数: int client_id,客户端ID
 * 返回值: NULL,查找失败
 *         非空，为客户端信息的指针
 */
struct client_info* __client_search(int client_id){
    struct client_info *p_client_i;
    list_for_each_entry(p_client_i,&client_i_h.list,list){  
        if(p_client_i->id == client_id){
            return p_client_i;
        }
    }
    return NULL;
}
/* 函数: client_info* client_search(int client_id)
 * 功能: 根据客户端ID查找客户端,访问列表加锁
 * 参数: int client_id,客户端ID
 * 返回值: NULL,查找失败
 *         非空，为客户端信息的指针
 */
struct client_info* client_search(int client_id){
    struct client_info *p_client_i;
    sem_wait(&client_list_mutex);
    p_client_i = __client_search(client_id);
    sem_post(&client_list_mutex);
    return p_client_i;
}
/* 函数: int client_wd_resume()
 * 功能: 喂狗
 * 参数: int client_id,客户端ID
 * 返回值: -1,无客户端
 *          1,喂狗成功
 */
int client_wd_resume(int client_id){
    struct client_info *p_client_i;
    sem_wait(&client_list_mutex);  //互斥访问客户端列表
    p_client_i = __client_search(client_id);  //__client_search(client_id),是内部调用函数，函数的中访问链表没有加锁
    if(p_client_i == NULL){
        sem_post(&client_list_mutex);
        return -1;
    }
    p_client_i->wd_cnt = WD_RESUME_CNT;
    sem_post(&client_list_mutex);
    return 1;
}

/* 函数: int client_wd_init()
 * 功能: 初始化看门狗
 * 参数: 
 * 返回值: -1,定时器设置失败
 *          1,初始化成功
 */
int client_wd_init(){
    struct itimerval tick;  //定时器配置结构体

    tick.it_value.tv_sec = 5;  //使能定时器之后，等待tv_sec秒之后，才真正启动定时器
    tick.it_value.tv_usec = 0;
    tick.it_interval.tv_sec = 1;  //周期为一秒
    tick.it_interval.tv_usec = 0;  

    //每计时一秒，调用一次client_wd_decline()函数
    signal(SIGALRM,client_wd_decline);  
    if(setitimer(ITIMER_REAL,&tick,NULL) != 0){   
        //设置定时器，类型为ITIMER_REAL(真实时间),成功设置则返回0
        return -1;
    }
    return 1;
}

/* 函数: int __client_del(client_info *p_client_i)
 * 功能: 根据客户端信息指针从内存上删除客户端，当然，在删除之前，把客户端相关的资
 *       源都释放掉
 * 参数: int client_id,需要移除的客户端的id
 * 返回值: -1,指针为空
 *          1,函数执行成功
 */
int __client_del(struct client_info *p_client_i){
    char name_temp[CLIENT_NAME_LENGTH];
    int id_temp;
    strcpy(name_temp,p_client_i->name);
    id_temp = p_client_i->id;
    if(p_client_i == NULL){
        return -1;
    }
    if(p_client_i->sockfd_cntl != 0){
        shutdown(p_client_i->sockfd_cntl,2);  //关闭套接字
    }
    free(p_client_i);
    syslog(LOG_DEBUG,"client(name:%s id:%d) delete complete",name_temp,id_temp);
    return 1;
}
/* 函数: int client_remove(int client_id)
 * 功能: 根据客户端ID移除客户端
 * 参数: int client_id,需要移除的客户端的id
 * 返回值: -1,出问题了
 *          1,函数执行成功
 */
int client_remove(int client_id){
    int err_ret;
    struct client_info *p_client_i,*n;
    sem_wait(&client_list_mutex);  //互斥访问客户端列表
    list_for_each_entry_safe(p_client_i,n,&client_i_h.list,list){  
        //可以对p_client_i进行删除操作
        if(p_client_i->id == client_id){
            list_del(&(p_client_i->list));  //从链表移除
            if(err_ret = __client_del(p_client_i) < 0){  //删除客户端
                syslog(LOG_DEBUG,"client_remove-->__client_del() error,err_ret:%d",err_ret);
                sem_post(&client_list_mutex);
                return err_ret;
            }   
        }    
    }
    sem_post(&client_list_mutex);
    return 1;
}


/* 函数: void client_wd_decline()
 * 功能: 看门狗计时器衰减(计数)函数
 * 参数: 
 * 返回值: -1,出问题了
 *          1,函数执行成功
 */
void client_wd_decline(){
    int err_ret;
    struct client_info *p_client_i,*n;
    sem_wait(&client_list_mutex);  //互斥访问客户端列表
    list_for_each_entry_safe(p_client_i,n,&client_i_h.list,list){  
        //syslog(LOG_DEBUG,"client:%s wd_cnt:%d",p_client_i->name,p_client_i->wd_cnt);
        //可以对p_client_i进行删除操作
        if(--(p_client_i->wd_cnt) == 0 && p_client_i->id != -1){  
            //计时到0且非链表头客户端,则移除客户端
            syslog(LOG_DEBUG,"client:%s timeout,removing it",p_client_i->name);
            list_del(&(p_client_i->list));  //从链表移除
            if(err_ret = __client_del(p_client_i) < 0){  //删除客户端
                syslog(LOG_DEBUG,"client_wd_decline()-->__client_del() error,err_ret:%d",err_ret);
                sem_post(&client_list_mutex);
                return;
            }   
        }    
    }
    sem_post(&client_list_mutex);
}

