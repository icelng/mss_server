#include "client.h"
#include "stdlib.h"
#include "stdio.h"
#include "arpa/inet.h"  //struct in_addr
#include "sys/socket.h"  //使用套接字
#include "sys/syslog.h"
#include "sys/time.h"
#include "sys/epoll.h"
#include "sys/types.h" //使用消息队列
#include "sys/ipc.h"  //使用消息队列
#include "sys/msg.h"  //使用消息队列
#include "time.h"
#include "signal.h"
#include "errno.h"   //错误号定义
//#include "list.h"   //使用链表
#include "string.h"
#include "fcntl.h"
#include "mysql.h"
#include "openssl/err.h"
#include "comunication.h"
#include "unistd.h"
#include "pthread.h"



struct usr_info{  //保存着验证信息
    char name[CLIENT_NAME_LENGTH];
    char passwd[MAX_PASSWD_LENGTH];
    int id;
    int lv;
    char pub_key[1024];  //通信所用的公钥
    char priv_key[1024];  //私钥
};
struct msg_struct{
    long msg_type;
    char msg_text[CLIENT_MSGTEXT_LENGTH];
};
MYSQL g_client_mysql;   //客户端数据库
sem_t client_list_mutex;  //互斥访问客户端列表
sem_t client_decrypt_mutex;
int g_msqid_dec;   //解密线程所需要的消息队列ID
struct client_info client_i_h;   //链表头客户端
int g_socket_epoll_fd;

/* 函数: int client_manage_init()
 * 功能: 初始化客户端管理
 * 参数: 
 * 返回值: -1,
 *          1,
 */
int client_manage_init(){
    int err_ret;
    client_i_h.id = -1;  //链表头的客户端的ID是-1
    unsigned long tid_irecv_thread;   //指令接收线程ID
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
    if((g_socket_epoll_fd = epoll_create(1)) == -1){
        syslog(LOG_DEBUG,"Epoll create error:%s",strerror(errno));
        return -1;
    }
    if(client_msq_init() == -1){
        syslog(LOG_DEBUG,"MSQ init error:%s",strerror(errno));
        return -1;
    }
    if(pthread_create(&tid_irecv_thread,NULL,client_recv_thread,NULL) < 0){  //创建专门接收信息的线程
        syslog(LOG_DEBUG,"Failed to create client_recv_thread,stop create client");
        return -1;
    }
    if(pthread_create(&tid_irecv_thread,NULL,client_dec_parse_thread,NULL) < 0){  //创建专门解密信息的线程
        syslog(LOG_DEBUG,"Failed to create client_dec_parse_thread,stop create client");
        return -1;
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
/* 函数名: int client_msq_init()
 * 功能: 消息队列初始化,获得消息队列ID
 * 参数:
 * 返回值: -1,
 *          1,
 */
int client_msq_init(){
    key_t key;
    if((key = ftok("./msqf_dec",1)) == -1){
        return -1;
    }
    if((g_msqid_dec = msgget(key,IPC_CREAT)) == -1){
        return -1;
    }
    return 1;
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

/* 函数名: struct client_info* client_get_ci(int client_id)
 * 功能: 根据客户端的ID，获取到客户端信息结构体的指针，该函数必须要与client_rele
 *       ase_struct()成对使用。
 * 参数: int client_id,客户端的ID
 * 返回值: NULL,内存里不存在客户端,也即客户端没有登录
 *         !=0,客户端信息结构体的指针
 */
struct client_info* client_get_ci(int client_id){
    struct client_info *p_c_i;
    sem_wait(&client_list_mutex);
    p_c_i = __client_search(client_id);
    if(p_c_i != NULL){
        sem_wait(&(p_c_i->queote_cnt_mutex));
        if(p_c_i->queote_cnt++ == 0){
            sem_wait(&(p_c_i->del_enable)); //拿取删除使能锁,不让别的线程删除该信息结构体
        }
        sem_post(&(p_c_i->queote_cnt_mutex));
    }
    sem_post(&client_list_mutex);
    return p_c_i;
}

/* 函数名: int client_release_ci(struct client_info *p_c_i)
 * 功能: 释放所拿到的客户端信息结构体,与client_get_ci()成对使用
 * 参数: struct client_info *p_c_i,所需释放的客户端信息结构体的指针
 * 返回值: -1,
 *          1,
 */
int client_release_ci(struct client_info *p_c_i){
    if(p_c_i == NULL){
        return -1;
    }
    sem_wait(&p_c_i->queote_cnt_mutex);
    if(--p_c_i->queote_cnt == 0){
        sem_post(&p_c_i->del_enable); //释放删除锁，允许其他线程删除此信息结构体
    }
    sem_post(&p_c_i->queote_cnt_mutex);
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

/* 函数名: int client_get_passwdandkey(char *src_str,char *passwd,char *pub_key)
 * 功能: 获取密码和公钥,str_str格式必须如下:passwd+pub_key
 * 参数: 
 * 返回值: -1,
 *          1,
 */
int client_get_passwdandkey(char *src_str,char *passwd,char *pub_key){
    int i = 0;
    while(src_str[i++] != '+'); 
    strncpy(passwd,src_str,i - 1);
    strcpy(pub_key,src_str + i);
    return 1;
}
/* 函数: int client_verify(int sockfd,int ip,struct usr_info)
 * 功能: 验证客户端,验证成功后，创建客户端
 * 说明: 验证步骤如下:
 *       1.接收需要登录的客户端的用户名，并且检验是否已经注册，如没有注册，则告知
 *         客户端用户名不存在,而不发送服务端的公钥
 *       2.如果用户名存在，则产生秘钥，而且与客户端交换公钥，具体步骤是：
 *          1)先给客户端发送服务端的公钥
 *          2)等待接收经过服务端公钥加密的客户端认证内容，认证内容包括:用户的密码
 *            +客户端公钥
 *       3.验证密码
 *       4.若验证成功，则告知成功，否则告知失败，并且说明原因
 * 参数: int sockfd,客户端的控制套接字
 *       int ip,客户端的ip
 *       struct usr_info,返回的验证信息
 * 返回值: -1,通信出问题
 *         -2,不存在账户(名称)
 *         -3,账户密码错误
 *         -4,重复登录
 *          1,验证成功
 */
int client_verify(int sockfd,struct in_addr ip,struct usr_info *p_usr_i){
    char passwd[MAX_RECV_STR_LENGTH] = {0};
    char recv_str[MAX_RECV_STR_LENGTH];
    char send_buf[MAX_RECV_STR_LENGTH];
    char local_pub_key[4096];
    char plain[MAX_RECV_STR_LENGTH];

    syslog(LOG_DEBUG,"ip:%s attempt to login",inet_ntoa(ip));
    if(com_recv_str(sockfd,recv_str,MAX_RECV_STR_LENGTH,1) < 0){
        return -1;
    }
    syslog(LOG_DEBUG,"login name:%s",recv_str);
    if(client_get_usrinfo(&g_client_mysql,recv_str,p_usr_i) == -1){
        syslog(LOG_DEBUG,"Client login failed,user name:%s does not exist",recv_str);
        if(com_send_str(sockfd,"error",strlen("error")) < 0)return -1;  //发送认证失败信息
        return -2;
    }
    syslog(LOG_DEBUG,"Genarating RSA key and share the pub key");
    rsa_gen_keys(RSA_KEY_LENGTH,local_pub_key,p_usr_i->priv_key);  //生成密钥对
    if(com_send_str(sockfd,local_pub_key,strlen(local_pub_key)) < 0)return -1;  //发送服务器的公钥
    if(com_recv_str(sockfd,recv_str,MAX_RECV_STR_LENGTH,1) < 0){  //接收客户端认证内容的密文
        return -1;
    }
    rsa_priv_decrypt(p_usr_i->priv_key,recv_str,plain);  //解密客户端发来的认证内容
    client_get_passwdandkey(plain,passwd,p_usr_i->pub_key);
    syslog(LOG_DEBUG,"Checking the passwd of %s,login passwd:%s",p_usr_i->name,passwd);
    if(strcmp(passwd,p_usr_i->passwd) != 0){
        syslog(LOG_DEBUG,"Client login failed:incorrect passwd");
        if(com_rsa_send(sockfd,p_usr_i->pub_key,"Client login failed:incorrect passwd") < 0)return -1;  //发送认证失败信息
        return -3;
    }
    if(client_search(p_usr_i->id) != NULL){
        syslog(LOG_DEBUG,"Client login failed:multiple login");
        if(com_rsa_send(sockfd,p_usr_i->pub_key,"Client login failed:multiple login") < 0)return -1;  //发送认证失败信息
        return -4;
    }
    syslog(LOG_DEBUG,"Client login successfully");
    rsa_pub_encrypt(p_usr_i->pub_key,"Client login successfully",send_buf);
    if(com_send_str(sockfd,send_buf,strlen(send_buf)) < 0)return -1;  //发送服务器的公钥
    return 1;
}
/* 函数名: void *client_recv_thread(void *arg)
 * 功能: 接收信息线程
 * 参数: void *arg,指向客户端信息结构体的指针
 * 返回值:
 */
void *client_recv_thread(){
    int oldtype;
    int n,i;
    int flags;
    int c_id;
    struct msg_struct msgs;
    char test_plain[4096];
    char test_plain2[4096];
    char cipher[4096];
    char recv_c;
    unsigned char aes_key[16];
    AES_KEY enc_key,dec_key;
    struct epoll_event *events;
    struct client_info *p_c_i;
    events = (struct epoll_event*)malloc(CLIENT_MAX_EPOLL_EVENTS);
    syslog(LOG_DEBUG,"Start client_recv_thread");
    pthread_setcancelstate(PTHREAD_CANCEL_ENABLE,&oldtype);  //设置线程可取消
    while(1){
        n = epoll_wait(g_socket_epoll_fd,events,CLIENT_MAX_EPOLL_EVENTS,-1);
        if(n == -1){
            syslog(LOG_DEBUG,"ERROR:client_recv_thread()-->epoll_wait():%s",strerror(errno));
            sleep(1);
            continue;
        }
        for(i = 0;i < n;i++){
            if(!(events[i].events & EPOLLIN) ||   //EPOLLIN是包括socket正常关闭的
                (events[i].events & EPOLLHUP) ||
                (events[i].events & EPOLLERR)){
                continue;
            }
            //获取到可读的sockfd对应的客户端信息结构体的指针
            c_id = events[i].data.u32;  //获得客户端ID
            p_c_i = client_get_ci(c_id);  //根据客户端ID获取到信息结构体的指针
            if(p_c_i == NULL){
                syslog(LOG_DEBUG,"ERROR:Client_recv_thread()-->client_get_ci():struct_info of client(id:%d) not found",c_id);
                continue;
            }
            flags = fcntl(p_c_i->sockfd,F_GETFL,0);
            fcntl(p_c_i->sockfd,F_SETFL,flags&O_NONBLOCK);  //设置成非阻塞模式
            while(1){
                if(recv(p_c_i->sockfd,&recv_c,1,MSG_DONTWAIT) <= 0){ //非阻塞接收
                    if(errno == 11)break;  //errno等于11表示没有可接收的了,可以直接退出循环
                    syslog(LOG_DEBUG,"client_recv_thread():failed to recv char:%s",strerror(errno));
                    break;
                }
                if(recv_c == '\n' && p_c_i->recv_is_datachar != 1){  //换行符表示结束
                    p_c_i->recv_buf[p_c_i->recv_index] = 0;
                    p_c_i->recv_index = 0;
                    p_c_i->recv_is_datachar = 0;
                    //msgs.msg_type = p_c_i->id;
                    //strcpy(msgs.msg_text,p_c_i->recv_buf);
                    //msgsnd(g_msqid_dec,&msgs,sizeof(struct msg_struct),IPC_NOWAIT);
                    //rsa_priv_decrypt(p_c_i->priv_key,p_c_i->recv_buf,test_plain);
                    //aes_cbc_enc(&p_c_i->aes_enc_key,test_plain,cipher);
                    //aes_cbc_dec(&p_c_i->aes_dec_key,cipher,test_plain2);
                    memset(test_plain,0,sizeof(test_plain));
                    aes_cbc_dec(&p_c_i->aes_dec_key,p_c_i->recv_buf,test_plain);
                    syslog(LOG_DEBUG,"Test msg:%s ",test_plain);
                    break;
                }else if(recv_c == '\r' && p_c_i->recv_is_datachar != 1){
                    p_c_i->recv_index = 0;
                }else{  
                    if(recv_c == 0x10 && p_c_i->recv_is_datachar != 1){  //为转义符号,说明下一个字符为数据，不是控制字符
                        p_c_i->recv_is_datachar = 1;
                    }else{
                        if(p_c_i->recv_is_datachar == 1){
                            p_c_i->recv_is_datachar = 0;
                        }
                        if(p_c_i->recv_index >= CLIENT_RECV_BUF_SIZE - 1){
                            p_c_i->recv_index = 0;
                            p_c_i->recv_is_datachar = 1;
                            syslog(LOG_DEBUG,"The recv_buf of client is overflow!");
                            break;
                        }
                        p_c_i->recv_buf[p_c_i->recv_index++] = recv_c;
                    }
                }
            }
            fcntl(p_c_i->sockfd,F_SETFL,flags&~O_NONBLOCK);  //设置成阻塞模式
            client_release_ci(p_c_i); //释放信息结构体
        }
    }
}

/* 函数名: int client_parse_info(char *info)
 * 功能: 解析接收到的消息,并且执行相关的命令
 * 参数: char *info原信息
 * 返回值: -1,出现错误
 *         1,调用成功
 */
int client_parse_do(char *info_src){
    char c_tmp;
    int i = 0;

    while(info_src[i++] != ':');

    return 1;
}
/* 函数名: void *client_dec_parse_thread(void *arg)
 * 功能: 客户端信息的解密和解析的线程
 * 参数:
 * 返回值:
 */
void *client_dec_parse_thread(){
    int ret_value;
    struct client_info *p_c_i;
    struct msg_struct msgs;
    char plain[MAX_RECV_STR_LENGTH];
    pthread_detach(pthread_self());  //线程结束时，资源由系统自动回收
    while(1){
        ret_value = msgrcv(g_msqid_dec,&msgs,sizeof(struct msg_struct),0,0);
        if(ret_value == -1){
            syslog(LOG_DEBUG,"ERROR:client_dec_parse()-->msgrcv():%s",strerror(errno));
            sleep(1);
            continue;
        }
        p_c_i = client_get_ci(msgs.msg_type);  //msg_type保存着客户端的ID 
        rsa_priv_decrypt(p_c_i->priv_key,msgs.msg_text,plain);
        client_release_ci(p_c_i);
        syslog(LOG_DEBUG,"Msg text:%s",plain);
    }
}
/* 函数: void client_create()
 * 功能: 创建客户端,而且把新建的客户端链入链表,作为线程被调用
 * 参数: 
 * 返回值: -1,
 *          1,
 */
void *client_create(void *p_client_i){
    struct usr_info u_i;
    struct client_info *p_new_client_i;
    struct epoll_event event;
    unsigned char aes_key[CLIENT_AES_KEY_LENGTH];
    int i;
    int err_ret;
    pthread_detach(pthread_self());  //线程结束时，资源由系统自动回收
    p_new_client_i = (struct client_info*)p_client_i;
    if((err_ret = client_verify(p_new_client_i->sockfd,p_new_client_i->ip,&u_i)) < 0){  //验证客户端
        shutdown(p_new_client_i->sockfd,2);  //关闭套接字
        free(p_new_client_i);
        return NULL;
    }
    /*复制认证好的信息*/
    p_new_client_i->id = u_i.id;
    strcpy(p_new_client_i->pub_key,u_i.pub_key);
    strcpy(p_new_client_i->priv_key,u_i.priv_key);
    strcpy(p_new_client_i->name,u_i.name);
    /*生成AESKEY，并且发给客户端*/
    syslog(LOG_DEBUG,"Genarating AES key and share the key"); 
    aes_gen_key(aes_key,CLIENT_AES_KEY_LENGTH);  //随机生成AES秘钥
    printf("\n");  //测试用代码，可删
    for(i = 0;i < 16;i++){  //
        printf(" 0x%x",aes_key[i]); //
    } //
    fflush(stdout); //
    AES_set_encrypt_key(aes_key,CLIENT_AES_KEY_LENGTH,&p_new_client_i->aes_enc_key);
    AES_set_decrypt_key(aes_key,CLIENT_AES_KEY_LENGTH,&p_new_client_i->aes_dec_key);
    com_rsa_send_aeskey(p_new_client_i->sockfd,p_new_client_i->pub_key,aes_key,CLIENT_AES_KEY_LENGTH);  //rsa公钥加密发送AESkey
    p_new_client_i->wd_cnt = WD_RESUME_CNT;  //初始化看门狗计数值
    p_new_client_i->queote_cnt = 0; //客户端信息结构体引用次数
    sem_init(&(p_new_client_i->queote_cnt_mutex),0,1);  //初始化引用次数互斥锁
    sem_init(&p_new_client_i->del_enable,0,1);  //初始化结构体删除锁
    /*链入链表*/
    sem_wait(&client_list_mutex);  //互斥访问链表
    my_list_add(&p_new_client_i->list,&client_i_h.list);  //把新创建的客户端链入链表
    sem_post(&client_list_mutex);  //互斥访问链表
    //free(p_new_client_i); //测试用代码，可删
    //return NULL;//测试用代码，可删
    /*配置epoll*/
    event.data.u32 = u_i.id;  
    event.events = EPOLLIN;  //设置读提醒，水平触发模式
    if(epoll_ctl(g_socket_epoll_fd,
                EPOLL_CTL_ADD,
                p_new_client_i->sockfd,
                &event) == -1){  //把客户端的socket加入epoll
        syslog(LOG_DEBUG,"Failed to add sockfd to epoll:%s",strerror(errno));
        return NULL;
    }
    syslog(LOG_DEBUG,"Client:%s create complete!",p_new_client_i->name);
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
    evp.sigev_notify = SIGEV_THREAD;   //线程通知方式,派驻线程
    evp.sigev_notify_function = client_wd_decline;//每计时一秒，调用一次client_wd_decline()函数
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
    //从epoll中移除socket,如果移除之前epoll_wait正在被调用，则正在调用的epoll_wait
    //还会监听被移除的socket的事件,下次调用epoll_wait()才不会监听该被移除的socke
    //t的事件
    if(epoll_ctl(g_socket_epoll_fd,EPOLL_CTL_DEL,p_client_i->sockfd,NULL) == -1){
        syslog(LOG_DEBUG,"Failed to remove sockfd from epoll");
    }
    if(p_client_i->sockfd != 0){
        shutdown(p_client_i->sockfd,2);  //关闭套接字
    }
    sem_wait(&p_client_i->del_enable);  //等待至可以删除
    free(p_client_i);
    syslog(LOG_DEBUG,"Client(name:%s id:%d) delete complete",name_temp,id_temp);
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
        }    
    }
    sem_post(&client_list_mutex);
    if(err_ret = __client_del(p_client_i) < 0){  //删除客户端
        syslog(LOG_DEBUG,"client_remove-->__client_del() error,err_ret:%d",err_ret);
        return err_ret;
    }   
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

