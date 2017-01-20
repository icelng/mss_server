/*嗯，这是参照Apache设计的一个内存池，或者说是Apache内存池的一个简易版，如果写完
 * 了之后觉得挺好用的，那我就继续添加树型结构。也挺担心到时就挺迟的了。
 */


#include "semaphore.h"   //没有移植的打算


#define MMPL_MAX_INDEX 10
/*下面是内存池的默认配置,内存池最大内存空间是200M*/
#define MMPL_MAX_FREE_INDEX_DEFAULT (1<<10)*100
#define MMPL_BOUNDARY_DEFAULT 2048
#define MMPL_BOUNDARY_INDEX 12
/*匹配最接近size的boundary的整数倍的整数，boundary必须为2次幂*/
#define MMPL_ALIGN(size,boundary) \
    (((size) + ((boundary) - 1)) & ~((boundary) - 1))
#define MMPL_ALIGN_DEFAULT(size) MMPL_ALIGN((size),MMPL_BOUNDARY_DEFAULT)

/*如同Apache的内存节点*/
struct mm_node{
    struct mm_node *next;  //下一个节点
    struct mm_node **ref;  //指向当前节点,*ref指向前驱节点的next
    unsigned int index;  //既可以表示节点内存的大小，也可以作为free数组的下标

};


struct mm_pool_s{
    /*内存池最大内存空间大小，防止向操作系统申请过多内存空间致使操作系统崩溃*/
    unsigned int max_free_index; //最大空闲内存
    unsigned int current_free_index;   //当前内存池空闲的内存
    struct mm_node use_head;  //正在使用的内存节点的链表头
    struct mm_node *free[MMPL_MAX_INDEX + 1];
    sem_t mutex;  //锁，用来互斥访问内存池
};

int mmpl_create(struct mm_pool_s **new_mmpl);
void* mmpl_getmem(struct mm_pool_s *p_mmpl,unsigned int size);
int mmpl_rlsmem(struct mm_pool_s *p_mmpl,void *rls_mmaddr);
