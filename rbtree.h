 /*参照算法导论红黑树那一章节写的*/ 

#define RB_RED 0
#define RB_BLACK 1

#define offsetof(TYPE, MEMBER) ((unsigned int) &((TYPE *)0)->MEMBER)   

#define container_of(ptr, type, member) ( { \
const typeof( ((type *)0)->member ) *__mptr = (ptr); \
(type *)( (char *)__mptr - offsetof(type,member) ); } )   

#define rb_entry(ptr, type, member) container_of(ptr, type, member)   

struct rb_node{  //红黑树节点
    unsigned char color; //颜色
    int key;  //关键字
    struct rb_node *lchild,*rchild,*parent; //孩子节点和父节点
};

struct rb_tree{
    struct rb_node *root;  //指向根节点
    struct rb_node nil;  //哨兵指针
};

int rb_init(struct rb_tree *p_rb_t);
struct rb_node* rb_search(struct rb_tree *p_rb_t,int key);
int rb_insert(struct rb_tree *p_rb_t,struct rb_node *pnew_rb_n);
int rb_delete(struct rb_tree *p_rb_t,struct rb_node *p_rm_n);
int rb_pre_traversal(struct rb_tree *p_rb_t,struct rb_node *p_root);
