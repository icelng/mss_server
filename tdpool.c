#include "tdpool.h"  
#include "pthread.h"
#include "unistd.h"
#include "stdio.h"
#include "string.h"
#include "mmpool.h"




/* 函数名: void tdpl_wktd_cleanup(void *arg)
 * 功能: worker线程清理函数
 * 参数: void *arg,指向线程信息结构体的指针
 * 返回值:
 */
void tdpl_wktd_cleanup(void *arg){
    struct tdpl_td_i *p_tti;
    p_tti = (struct tdpl_td_i*)arg;
    sem_destroy(&p_tti->run); //释放信号量
    
}

/* 函数名: void tdpl_mastertd_cleanup(void *arg)
 * 功能: master线程清理函数
 * 参数:
 * 返回值:
 */
void tdpl_mastertd_cleanup(void *arg){
    
}


/* 函数名: void *tdpl_destroy_thread(void *arg)
 * 功能: 线程池销毁线程
 * 参数: void *arg,指向线程池结构体的指针
 * 返回值:
 */
void *tdpl_destroy_thread(void *arg){
    struct tdpl_s *pts;
    struct tdpl_td_i *p_tti;
    int i;
    pts = (struct tdpl_s *)arg;
    /*取消所有的worker线程*/
    for(i = 0;i < pts->thread_num;i++){
        p_tti = &pts->tti_array[i];
        pthread_cancel(p_tti->tid); //给worker线程发送取消信号
        /*因为worker线程的取消点设置在sem_wait()的后面，所以得执行下面的语句*/
        sem_post(&p_tti->run);
        pthread_join(p_tti->tid,NULL); //等待线程取消完毕
    }
    /*销毁各种信号量*/
    sem_destroy(&pts->req_list_mutex);
    sem_destroy(&pts->avali_list_mutex);
    sem_destroy(&pts->req_n);
    sem_destroy(&pts->avali_td_n);
    sem_destroy(&pts->req_n_emty);
    /*销毁内存池*/
    mmpl_destroy(pts->mmpl);
}

/* 函数名: int tdpl_destroy(struct tdpl_s *pts)
 * 功能: 销毁线程池
 * 参数: struct tdpl_s *pts,指向线程池结构体的指针
 * 返回值: -1,
 *          1,
 */
int tdpl_destroy(struct tdpl_s *pts){
    unsigned long tid;

    if(pts == NULL){
        return -1;
    }
    pthread_cancel(pts->master_tid); //给master线程发送线程取消信号
    /*因为master线程取消点是设置在等待两个信号量之后的位置，所以得进行两个V操作*/
    sem_post(&pts->avali_td_n);
    sem_post(&pts->req_n);
    pthread_join(pts->master_tid,NULL); //等待master线程取消完毕
    /*下面是消耗完请求队列的位置，即是拒绝指定函数的调用请求*/
    while(sem_trywait(&pts->req_n_emty) != -1); 
    /*启动线程池销毁线程，下来的清理工作交由线程tdpl_destroy_thread负责*/
    if(pthread_create(&tid,NULL,tdpl_destroy_thread,pts) == -1){ 
        return -1;
    }
    return 1;
}

/* 函数名: void *tdpl_worker_thread(*arg)
 * 功能: 调用函数的线程
 * 参数: void *arg,指向该线程信息结构体的指针
 * 返回值:
 */
void *tdpl_worker_thread(void *arg){
    struct tdpl_td_i *p_tti; //指向线程信息结构体的指针
    struct tdpl_s *pts;  //指向线程所属线程池的结构体
    pthread_setcancelstate(PTHREAD_CANCEL_ENABLE,NULL); //设置线程可被取消
    p_tti = (struct tdpl_td_i *)arg;
    pts = p_tti->p_tdpl_s;
    pthread_cleanup_push(tdpl_wktd_cleanup,p_tti);
    while(1){
        sem_wait(&p_tti->run); //等待至可以调用函数
        pthread_testcancel(); //设置线程取消点,在销毁线程池时，线程会被取消
        /*下一条语句为调用函数*/
        (p_tti->call_fun)(p_tti->arg); 
        /* 函数执行完了之后，把该线程的信息结构体插入可用队列中,并且把参数所占用
         * 的内存空间还给内存池*/
        /* 有时候在调用函数的过程中接收到了线程取消信号,所以在函数执行完了之后马
         * 上取消线程。*/
        pthread_testcancel(); 
        mmpl_rlsmem(pts->mmpl,p_tti->arg);
        p_tti->arg = NULL;
        sem_wait(&pts->avali_list_mutex); //互斥访问可用线程链表
        list_add_tail(&p_tti->list,&pts->avali_list_h); //插入队尾
        sem_post(&pts->avali_list_mutex);
        sem_post(&pts->avali_td_n);  //可利用线程数+1
    }
    pthread_cleanup_pop(0);
    
}

/* 函数名: void *tdpl_master_thread(void *arg)
 * 功能: 管理worker线程的线程
 * 参数: void *arg,指向线程池结构体的指针
 * 返回值:
 */
void *tdpl_master_thread(void *arg){
    struct tdpl_s *pts;
    struct tdpl_td_i *p_tti;  //线程信息结构体
    struct tdpl_req_node *p_trn;
    pthread_setcancelstate(PTHREAD_CANCEL_ENABLE,NULL); //设置线程可被取消
    pts = (struct tdpl_s*)arg;
    pthread_cleanup_push(tdpl_mastertd_cleanup,NULL);
    while(1){
        sem_wait(&pts->avali_td_n); //等待至有可用线程
        sem_wait(&pts->req_n);  //等待有函数调用请求
        pthread_testcancel();  //设置线程取消点，销毁线程时，该线程会在这被取消
        sem_wait(&pts->avali_list_mutex); //互斥访问可用线程链表
        /*从可用线程链表中取出一个线程*/
        p_tti = list_entry(pts->avali_list_h.next,struct tdpl_td_i,list);
        list_del(&p_tti->list);
        sem_post(&pts->avali_list_mutex);
        sem_wait(&pts->req_list_mutex); //互斥访问请求队列
        /*从请求队列中取出一个请求*/
        p_trn = list_entry(pts->req_list_h.next,struct tdpl_req_node,list);
        list_del(&p_trn->list);
        sem_post(&pts->req_list_mutex);
        sem_post(&pts->req_n_emty);  //请求队列位置+1
        p_tti->call_fun = p_trn->call_fun;
        p_tti->arg = p_trn->arg;
        mmpl_rlsmem(pts->mmpl,p_trn);
        sem_post(&p_tti->run); //告知线程开始调用函数
    }
    pthread_cleanup_pop(0);
}




/* 函数名: struct tdpl_s* tdpl_create(unsigned int thread_num,unsigned int max_wait_n)
 * 功能: 创建线程池
 * 参数: int thread_num,线程池的线程数
 *       int max_wait_n,最大请求调用线程的等待数
 * 返回值: NULL,创建出错
 *        !NULL,线程池结构体指针
 */
struct tdpl_s* tdpl_create(int thread_num,int max_wait_n){
    struct mm_pool_s *tdpl_mmpl = NULL;
    struct tdpl_s *p_new_tdpl_s;
    struct tdpl_td_i *p_tti; //指向线程池的线程信息结构体
    int i;
     //创建内存池
    if(mmpl_create(&tdpl_mmpl) == -1){
        goto err1_ret;
    }
    /*创建线程池结构体*/
    p_new_tdpl_s = (struct tdpl_s *)mmpl_getmem(tdpl_mmpl,sizeof(struct tdpl_s));
    if(p_new_tdpl_s == NULL){
        goto err2_ret;
    }
    /*为worker线程信息结构体数组安排内存空间*/
    p_new_tdpl_s->tti_array = mmpl_getmem(tdpl_mmpl,thread_num*sizeof(struct tdpl_td_i));
    if(p_new_tdpl_s->tti_array == NULL){
        goto err2_ret;
    }
    p_new_tdpl_s->mmpl = tdpl_mmpl;
    p_new_tdpl_s->thread_num = thread_num;
    p_new_tdpl_s->max_wait_n = max_wait_n;
    /*初始化各种信号量*/
    sem_init(&p_new_tdpl_s->req_list_mutex,0,1);
    sem_init(&p_new_tdpl_s->avali_list_mutex,0,1);
    sem_init(&p_new_tdpl_s->avali_td_n,0,thread_num);
    sem_init(&p_new_tdpl_s->req_n,0,0);
    sem_init(&p_new_tdpl_s->req_n_emty,0,max_wait_n);
    INIT_LIST_HEAD(&p_new_tdpl_s->avali_list_h); //初始化链表
    INIT_LIST_HEAD(&p_new_tdpl_s->req_list_h);
    /*启动master线程*/
    if(pthread_create(&p_new_tdpl_s->master_tid,NULL,tdpl_master_thread,p_new_tdpl_s) == -1){ 
        goto err3_ret;
    }
    /*建立线程*/
    for(i = 0;i < thread_num;i++){
        p_tti = &p_new_tdpl_s->tti_array[i];
        list_add_tail(&p_tti->list,&p_new_tdpl_s->avali_list_h); //加入队列
        p_tti->p_tdpl_s = p_new_tdpl_s; //设置所属线程池
        sem_init(&p_tti->run,0,0);//初始化线程继续运行的信号量
        if(pthread_create(&p_tti->tid,NULL,tdpl_worker_thread,p_tti) == -1){ 
            sem_destroy(&p_tti->run);
            goto err4_ret;
        }
    }
    return p_new_tdpl_s;

    /*下面是线程池的创建发生了错误之后需要处理的事情的代码*/

err4_ret:
    while(i-- != 0){
        p_tti = list_entry(p_new_tdpl_s->avali_list_h.next,struct tdpl_td_i,list);
        list_del(&p_tti->list);
        sem_destroy(&p_tti->run);
    }
err3_ret:
    sem_destroy(&p_new_tdpl_s->req_list_mutex);
    sem_destroy(&p_new_tdpl_s->avali_list_mutex);
    sem_destroy(&p_new_tdpl_s->req_n);
    sem_destroy(&p_new_tdpl_s->avali_td_n);
    sem_destroy(&p_new_tdpl_s->req_n_emty);
err2_ret:
    mmpl_destroy(tdpl_mmpl);
err1_ret:
    return NULL;
}


/* 函数名: int tdpl_call_fun(struct tdpl_s *pts,void (*call_fun)(void *arg),void *arg,int arg_size)
 * 功能: 使用线程池的一个线程来调用函数,参数所占用的内存空间的释放由线程池负责，
 *       不用被调用的函数来负责。
 * 参数: struct tdpl_s *pts,指向线程池结构体的指针
 *       void (*call_fun)(void *arg),需要调用的函数地址
 *       void *arg,需要调用的函数的参数首地址
 *       int arg_size,参数大小
 * 返回值: 1,
 *        -1,
 */
int tdpl_call_fun(struct tdpl_s *pts,void (*call_fun)(void *arg),void *arg,int arg_size){
    struct tdpl_req_node *p_trn;
    void *call_arg;
    if(sem_trywait(&pts->req_n_emty) == -1){ //查看请求队列是否已满
        return -1; //如果满则放弃请求
    }
    /*为请求队列节点申请内存空间*/
    p_trn = mmpl_getmem(pts->mmpl,sizeof(struct tdpl_req_node));
    /*为需调用的函数的参数申请内存空间，释放也由线程池负责*/
    call_arg = mmpl_getmem(pts->mmpl,arg_size);
    memcpy(call_arg,arg,arg_size);
    if(p_trn == NULL || arg == NULL){
        return -1;
    }
    p_trn->call_fun = call_fun;
    p_trn->arg = call_arg;
    sem_wait(&pts->req_list_mutex);
    list_add_tail(&p_trn->list,&pts->req_list_h);
    sem_post(&pts->req_list_mutex);
    sem_post(&pts->req_n);
    return 1;
}

