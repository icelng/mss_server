#include "mmpool.h"
#include "stdlib.h"
#include "string.h"
#include "stdio.h"
#include "unistd.h"



/* 函数名: int mmpl_create(struct mm_pool_s *new_mmpl)
 * 功能: 创建新的内存池
 * 参数: struct mm_pool_s *new_mmpl,内存池结构体指针
 * 返回值: -1,
 *          1,
 */
int mmpl_create(struct mm_pool_s **new_mmpl){
    *new_mmpl = (struct mm_pool_s *)malloc(sizeof(struct mm_pool_s));
    memset((void*)*new_mmpl,0,sizeof(struct mm_pool_s));
    (*new_mmpl)->max_free_index = MMPL_MAX_FREE_INDEX_DEFAULT;
    (*new_mmpl)->current_free_index = 0;
    sem_init(&(*new_mmpl)->mutex,0,1);  //初始化锁
    return 1;
}



/* 函数名: int mmpl_destroy(struct mm_pool_s *p_mmpl)
 * 功能: 销毁内存池，把内存空间归还给操作系统
 * 参数: struct mm_pool_s *p_mmpl,需要销毁的内存池的结构体
 * 返回值: -1,
 *          1,
 */
int mmpl_destroy(struct mm_pool_s *p_mmpl){
    return 1;
}


/* 函数名: int mmpl_list_insert(struct mm_node *pre_p_n,struct mm_node *p_insert_n)
 * 功能: 把内存节点插入到链表
 * 参数: struct mm_node *pre_p_mm_n,需要插入位置的前驱节点指针
 *       struct mm_node *p_insert_mm_n,需要插入的节点的指针
 * 返回值: -1,
 *          1,
 */
int mmpl_list_insert(struct mm_node *p_pre_n,struct mm_node *p_insert_n){
    if(p_pre_n == NULL || p_insert_n == NULL){
        return -1;
    }
    p_insert_n->next = p_pre_n->next;
    p_insert_n->pre = p_pre_n;
    p_pre_n->next->pre = p_insert_n;
    p_pre_n->next = p_insert_n;
    return 1;
}

/* 函数名: int mmpl_list_remove(struct mm_node *p_rm_node)
 * 功能: 从节点所在的链表中移除该节点
 * 参数: struct mm_node *p_rm_node,需要移除的节点
 * 返回值: -1,
 *          1,
 */
int mmpl_list_remove(struct mm_node *p_rm_node){
    if(p_rm_node->next == p_rm_node){  //如果链表就只有该节点，则无法删除
        return -1;
    }
    p_rm_node->next->pre = p_rm_node->pre;
    p_rm_node->pre->next = p_rm_node->next;
    return 1;
}


/* 函数名: void* mmpl_getmem(struct mm_pool_s *p_mmpl,unsigned int size)
 * 功能: 从指定的内存池里获取到内存空间
 * 参数: struct mm_pool_s *p_mmpl,内存池结构体指针
 *       int size,需要申请空间的大小
 * 返回值: NULL,获取失败
 *         !=NULL,获取到的内存地址
 */
void* mmpl_getmem(struct mm_pool_s *p_mmpl,unsigned int size){
    unsigned int align_size;
    unsigned int index;
    struct mm_node *p_mm_n;

    align_size = MMPL_ALIGN_DEFAULT(size);  //默认2K对齐
    index = align_size/MMPL_BOUNDARY_DEFAULT;
    sem_wait(&p_mmpl->mutex);  //互斥操作内存池
    if((p_mm_n = p_mmpl->free[index]) == NULL){  
        /*如果free数组中没有相应的内存节点，则向操作系统申请*/
        p_mm_n = (struct mm_node *)malloc(align_size + sizeof(struct mm_node));
        if(p_mm_n == NULL){//向操作系统申请内存失败
            sem_post(&p_mmpl->mutex);
            return NULL;
        }
        p_mm_n->pre=p_mm_n->next = p_mm_n;
        p_mm_n->index = index;
    }else if(p_mm_n->next == p_mm_n){
        /*如果free[index](规则链表)链表只有一个节点*/
        p_mmpl->free[index] = NULL;
        p_mmpl->current_free_index -= index; //空闲内存空间减少
    }else{
        p_mmpl->free[index] = p_mm_n->next;
        mmpl_list_remove(p_mm_n);
        p_mmpl->current_free_index -= index; //空闲内存空间减少
    }
    sem_post(&p_mmpl->mutex);
    return (void *)p_mm_n + sizeof(struct mm_node);
}


/* 函数名: int mmpl_rlsmem(struct mm_pool_s *p_mmpl,void *rls_mmaddr)
 * 功能: 把从内存池申请到的内存空间还给内存池，如果还给内存池之后空闲的内存空间超
 *       过了内存池所设置的最大空闲内存，则把将要归还给内存池的内存空间直接还给操
 *       作系统。
 * 参数: strcut mm_pool_s *p_mmpl,内存池
 *       void *rls_mmaddr,需要释放的内存空间的首地址
 * 返回值:
 */
int mmpl_rlsmem(struct mm_pool_s *p_mmpl,void *rls_mmaddr){
    struct mm_node *p_mm_n;  
    unsigned int index;

    p_mm_n = rls_mmaddr - sizeof(struct mm_node);//获得内存节点的首地址
    index = p_mm_n->index;
    sem_wait(&p_mmpl->mutex);  //互斥操作内存池
    if(p_mmpl->current_free_index + index > p_mmpl->max_free_index){
        /*如果归还之后内存池的空闲空间超过了最大空闲空间大小则直接归还给操作系统*/
        free(p_mm_n);
        sem_post(&p_mmpl->mutex);  //互斥操作内存池
        return 1;
    }
    if(p_mmpl->free[index] == NULL){
        p_mm_n->pre=p_mm_n->next = p_mm_n;
        p_mmpl->free[index] = p_mm_n;
    }else{
        if(mmpl_list_insert(p_mmpl->free[index],p_mm_n) == -1){
            printf("error\n");
        }
    }
    p_mmpl->current_free_index += index;
    sem_post(&p_mmpl->mutex);  //互斥操作内存池
    return 1;
}
