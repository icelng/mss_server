#include "client.h"
#include "stdlib.h"
#include "stdio.h"
#include "arpa/inet.h"  //struct in_addr
#include "sys/socket.h"  //使用套接字
#include "sys/syslog.h"
#include "sys/time.h"
#include "time.h"
#include "signal.h"
#include "errno.h"   //错误号定义
#include "semaphore.h"  //使用信号量
//#include "list.h"   //使用链表
#include "string.h"
#include "fcntl.h"
#include "mysql.h"
#include "rsa.h"
#include "openssl/err.h"
#include "comunication.h"
#include "unistd.h"
#include "pthread.h"



struct usr_info{  //保存着验证信息
    char name[CLIENT_NAME_LENGTH];
    char passwd[MAX_PASSWD_LENGTH];
    int id;
    int lv;
    char pub_key[4096];  //通信所用的公钥
    char priv_key[4096];  //私钥
};

MYSQL g_client_mysql;   //客户端数据库
sem_t client_list_mutex;  //互斥访问客户端列表
sem_t client_decrypt_mutex;
struct client_info client_i_h;   //链表头客户端
/* 函数: int client_manage_init()
 * 功能: 初始化客户端管理
 * 参数: 
 * 返回值: -1,
 *          1,
 */
int client_manage_init(){
    int err_ret;
    client_i_h.id = -1;  //链表头的客户端的ID是-1
    INIT_LIST_HEAD(&client_i_h.list);   //初始化客户端链表
    sem_init(&client_list_mutex,0,1);  //初始化信号量(锁)
    rsa_init();
    if (client_wd_init() == -1){   //初始化看门狗
        syslog(LOG_DEBUG,"client_manage_init-->client_wd_init error:return -1");
        return -1;
    }
    if((err_ret = client_mysql_connect(&g_client_mysql)) < 0){  //连接数据库
        return err_ret;
    }   
    return 1;
}

/* 函数名: int client_mysql_connect()
 * 功能: 连接数据库
 * 参数:
 * 返回值:
 */
int client_mysql_connect(MYSQL *p_mysql){
    

    if(mysql_init(p_mysql) == NULL){   //初始化mysql
        //初始化失败
        syslog(LOG_DEBUG,"init client mysql failed:%s",mysql_error(p_mysql));
        return -1;
    }
    if(NULL == mysql_real_connect(p_mysql, //链接数据库
                "localhost",
                "root",
                "snivwkbsk123",
                "muma",
                0,
                NULL,
                0
                )){
        //链接失败
        syslog(LOG_DEBUG,"connect to mysql failed:%s",mysql_error(p_mysql));
        return -2;
    }
    syslog(LOG_DEBUG,"connect to mysql successfully");
    
}
/* 函数名: int client_show_users()
 * 功能: 展示已经注册的用户
 * 参数:
 * 返回值:
 */
int client_users_show(MYSQL *p_client_mysql){
    MYSQL_RES *res = NULL;
    MYSQL_ROW row;
    char *query_str = NULL;
    int rows,fields;
    int err_ret;
    int i;

    syslog(LOG_DEBUG,"Showing all users");
    query_str = "select * from client_info";
    err_ret = mysql_real_query(p_client_mysql,query_str,strlen(query_str));
    if(err_ret != 0){
        syslog(LOG_DEBUG,"error:client_users_show()-->mysql_real_query():%s",mysql_error(p_client_mysql));
        return -1;
    }
    res = mysql_store_result(p_client_mysql);
    if(res == NULL){
        syslog(LOG_DEBUG,"error:client_users_show()-->mysql_store_result():%s",mysql_error(p_client_mysql));
        return -1;
    }
    rows = mysql_num_rows(res);
    syslog(LOG_DEBUG,"The total rows is:%d",rows);
    fields = mysql_num_fields(res);
    syslog(LOG_DEBUG,"The total fields is:%d",rows);
    while(row = mysql_fetch_row(res)){
        for(i = 0;i < fields;i++){
            syslog(LOG_DEBUG,"%s",row[i]);
        }
    }
    return 1;
}


/* 函数名: int client_get_usrinfo(MYSQL *p_client_mysql,char *usr_name)
 * 功能: 根据用户的名称获得用户的信息
 * 参数: MYSQL,指向数据库结构体的指针
 *       char *usr_name,需要查找用户的信息的名称
 *       struct usr_info *p_u_i,指向用户信息结构体的指针，用来保存查询到的用户信息
 * 返回值: -1,获取用户信息失败
 *          1,获取用户信息成功
 */
int client_get_usrinfo(MYSQL *p_client_mysql,char *usr_name,struct usr_info *p_u_i){
    MYSQL_RES *res = NULL;
    MYSQL_ROW row;
    char query_str[MAX_QUERY_STR_LENGTH];
    int err_ret;

    sprintf(query_str,"select * from client_info where name=\"%s\"",usr_name);
    err_ret = mysql_real_query(p_client_mysql,query_str,strlen(query_str));
    if(err_ret != 0){
        syslog(LOG_DEBUG,"error:client_get_usrinfo()-->mysql_real_query():%s",mysql_error(p_client_mysql));
        return -1;
    }
    res = mysql_store_result(p_client_mysql);
    if(res == NULL){
        syslog(LOG_DEBUG,"error:client_get_usrinfo()-->mysql_store_result():%s",mysql_error(p_client_mysql));
        return -1;
    }
    row = mysql_fetch_row(res);
    if(row == NULL){
        syslog(LOG_DEBUG,"user name %s not found",usr_name);
        return -1;
    }
    p_u_i->id = atoi(row[0]);
    strcpy(p_u_i->name,row[1]);
    strcpy(p_u_i->passwd,row[2]);
    p_u_i->lv = atoi(row[3]);

    return 1;
}

/* 函数: int client_verify(int sockfd_cntl,int ip,struct usr_info)
 * 功能: 验证客户端,验证成功后，创建客户端
 * 参数: int sockfd_cntl,客户端的控制套接字
 *       int ip,客户端的ip
 *       struct usr_info,返回的验证信息
 * 返回值: -1,通信出问题
 *         -2,不存在账户(名称)
 *         -3,账户密码错误
 *          1,验证成功
 */
int client_verify(int sockfd_cntl,struct in_addr ip,struct usr_info *p_usr_i){
    char passwd[MAX_RECV_STR_LENGTH] = {0};
    char recv_str[MAX_RECV_STR_LENGTH];

    syslog(LOG_DEBUG,"ip:%s attempt to login",inet_ntoa(ip));
    if(com_recv_str(sockfd_cntl,recv_str,MAX_RECV_STR_LENGTH,1) < 0){
        return -1;
    }
    syslog(LOG_DEBUG,"login name:%s",recv_str);
    if(client_get_usrinfo(&g_client_mysql,recv_str,p_usr_i) == -1){
        syslog(LOG_DEBUG,"Client login failed");
        return -2;
    }
    syslog(LOG_DEBUG,"Genarating RSA key");
    rsa_gen_keys(RSA_KEY_LENGTH,p_usr_i->pub_key,p_usr_i->priv_key);
    com_send_str(sockfd_cntl,p_usr_i->pub_key,strlen(p_usr_i->pub_key));
    syslog(LOG_DEBUG,"Checking the passwd of %s",p_usr_i->name);
    if(com_recv_str(sockfd_cntl,recv_str,MAX_RECV_STR_LENGTH,1) < 0){
        return -1;
    }
    rsa_priv_decrypt(p_usr_i->priv_key,recv_str,passwd);
    if(strcmp(passwd,p_usr_i->passwd) != 0){
        syslog(LOG_DEBUG,"Client login failed:incorrect passwd");
        return -3;
    }
    syslog(LOG_DEBUG,"Client login successfully");
    return 1;
}
/* 函数: void client_create()
 * 功能: 创建客户端,而且把新建的客户端链入链表,作为线程调用
 * 参数: 
 * 返回值: -1,
 *          1,
 */
void *client_create(void *p_client_i){
    struct usr_info u_i;
    struct client_info *p_new_client_i;
    int err_ret;
    pthread_detach(pthread_self());
    p_new_client_i = (struct client_info*)p_client_i;

    if((err_ret = client_verify(p_new_client_i->sockfd_cntl,p_new_client_i->ip,&u_i)) < 0){  //验证客户端
        syslog(LOG_DEBUG,"client_verify failed,faield number:%d",err_ret);  //验证失败，退出
        shutdown(p_new_client_i->sockfd_cntl,2);  //关闭套接字
        free(p_new_client_i);
        return NULL;
    }
    p_new_client_i->id = u_i.id;
    p_new_client_i->wd_cnt = WD_RESUME_CNT;
    strcpy(p_new_client_i->name,u_i.name);
    sem_wait(&client_list_mutex);  //互斥访问链表
    my_list_add(&p_new_client_i->list,&client_i_h.list);  //把新创建的客户端链入链表
    sem_post(&client_list_mutex);  //互斥访问链表
    syslog(LOG_DEBUG,"client:%s create complete!",p_new_client_i->name);
    return &*p_client_i;
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
    timer_t timerid;
    struct sigevent evp;
    memset(&evp,0,sizeof(struct sigevent));
    evp.sigev_value.sival_int = 111;
    evp.sigev_notify = SIGEV_THREAD;   //县城通知方式,派驻线程
    evp.sigev_notify_function = client_wd_decline;
    if(timer_create(CLOCK_REALTIME,&evp,&timerid) == -1){
        syslog(LOG_DEBUG,"Failed to create timer ");
        return -1;
    }

    tick.it_value.tv_sec = 1;  //使能定时器之后，等待tv_sec秒之后，才真正启动定时器
    tick.it_value.tv_usec = 0;
    tick.it_interval.tv_sec = 1;  //周期为一秒
    tick.it_interval.tv_usec = 0;  

    if(timer_settime(timerid,0,&tick,NULL) == -1){
        syslog(LOG_DEBUG,"Failed to set timer");
        return -1;
    }
    //每计时一秒，调用一次client_wd_decline()函数
    //signal(SIGALRM,client_wd_decline);  
    //if(setitimer(ITIMER_REAL,&tick,NULL) != 0){   
    //    //设置定时器，类型为ITIMER_REAL(真实时间),成功设置则返回0
    //    return -1;
    //}
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
void client_wd_decline(union sigval v){
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

