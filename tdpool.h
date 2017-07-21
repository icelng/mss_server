#include "list.h"
#include "semaphore.h"



/*请求队列的节点*/
struct tdpl_req_node{
    void *arg;
    void (*call_fun)(void *arg); //需要调用的函数
    struct list_head list; //嵌入到请求链表中
};

/*线程池结构体*/
struct tdpl_s{
    struct mm_pool_s *mmpl;  //线程池中会使用到的内存池
    int thread_num;  //线程池拥有的线程数
    int max_wait_n;  //最大等待数,或者说是请求链表的最大节点数
    unsigned long master_tid; //master线程id
    struct tdpl_td_i *tti_array; //线程信息结构体数组
    struct list_head avali_list_h;  //可用线程的链表头
    struct list_head req_list_h; //请求链表头
    sem_t avali_td_n;   //用信号量表示可用线程数
    sem_t req_n;   //用信号量表示当前请求数
    sem_t req_n_emty; //还可以往请求队列插入请求的数量
    sem_t avali_list_mutex; //可用线程链表的互斥信号量
    sem_t req_list_mutex;   //请求链表的互斥信号量
};

/*线程池中的线程信息结构体*/
struct tdpl_td_i{
    struct tdpl_s *p_tdpl_s; //线程所属的线程池的结构体
    unsigned long tid; //线程id
    void (*call_fun)(void *arg); //需要调用的函数
    void *arg;  //线程调用函数的参数
    struct list_head list; //会嵌入到可用线程链表
    sem_t run;  //用来告知线程开始调用函数
};
/*创建线程池*/
struct tdpl_s* tdpl_create(int thread_num,int max_wait_n);
/*使用线程池中的一个线程来调用指定的函数*/
int tdpl_call_fun(struct tdpl_s *pts,void (*call_fun)(void *arg),void *arg,int arg_size);
int tdpl_destroy(struct tdpl_s *pts);
void *tdpl_worker_thread(void *arg);
void *tdpl_master_thread(void *arg);
