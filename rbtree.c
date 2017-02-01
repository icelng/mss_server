#include "rbtree.h"
#include "stdio.h"


/* 函数名: int rb_init(struct rb_tree *rb_t)
 * 功能: 初始化红黑树
 * 参数: struct rb_tree *rb_t,红黑树结构体
 * 返回值: -1,
 *          1,
 */
int rb_init(struct rb_tree *p_rb_t){
    if(p_rb_t == NULL){
        return -1;
    }
    p_rb_t->nil.color = RB_BLACK;  //哨兵节点的color属性是black
    p_rb_t->root = &p_rb_t->nil;
    return 1;
}


/* 函数名: struct rb_node* rb_search(struct rb_tree *p_rb_t,int key)
 * 功能: 查找节点
 * 参数: struct rb_tree *p_rb_t,红黑树结构体指针
 *       int key,需要查找节点的关键字
 * 返回值: NULL,找不到节点
 *        !NULL,红黑树节点地址
 */
struct rb_node* rb_search(struct rb_tree *p_rb_t,int key){
    struct rb_node *p_rb_n;
    struct rb_node *nil;  //哨兵节点
    if(p_rb_t == NULL){
        return NULL;
    }
    p_rb_n = p_rb_t->root;  //从根节点开始比较
    nil = &p_rb_t->nil;
    while((p_rb_n != nil) && (p_rb_n->key != key)){
        if(key < p_rb_n->key){
            p_rb_n = p_rb_n->lchild;
        }else{
            p_rb_n = p_rb_n->rchild;
        }
    }
    if(p_rb_n == nil)return NULL;
    return p_rb_n;
}


/* 函数名: int rb_left_rotate(struct rb_tree *p_rb_t,struct rb_node *p_n)
 * 功能: 左旋
 * 参数: struct rb_tree *p_rb_t,红黑树结构体指针
 *       struct rb_node *p_n,需要旋转的节点
 * 返回值: -1,
 *          1,
 */
int rb_left_rotate(struct rb_tree *p_rb_t,struct rb_node *p_n){
    struct rb_node *nil,*p_ori_rchild;
    if(p_rb_t == NULL || p_n == NULL){
        return -1;
    }
    nil = &p_rb_t->nil;
    /*如果需要左旋的节点的右子节点是哨兵节点，则无法旋转*/
    if(p_n->rchild == nil){  
        return -1;
    }
    p_ori_rchild = p_n->rchild;
    p_n->rchild = p_ori_rchild->lchild;
    /* 不改变站哨节点的父指针，因为在进行删除操作之后做修复工作可能是从站哨节点开
     * 始的，如果旋转工作改变站哨节点的父指针，会使得修复工作可能出现紊乱*/
    if(p_ori_rchild->lchild != nil){ //不改变站哨节点的父指针
        p_ori_rchild->lchild->parent = p_n;
    }
    p_ori_rchild->parent = p_n->parent;
    if(p_n->parent == nil){ //如果p_n是根节点
        p_rb_t->root = p_ori_rchild;
    }else if(p_n == p_n->parent->lchild){//如果p_n是父节点的左子女
        p_n->parent->lchild = p_ori_rchild;
    }else{ //如果p_n是父节点的右子女
        p_n->parent->rchild = p_ori_rchild;
    }
    p_ori_rchild->lchild = p_n;
    p_n->parent = p_ori_rchild;
    return 1;
}

/* 函数名: int rb_right_rotate(struct rb_tree *p_rb_t,struct rb_node *p_n)
 * 功能: 右旋
 * 参数:  struct rb_tree *p_rb_t,红黑树结构体指针
 *        struct rb_node *p_n,需要旋转的节点
 * 返回值: -1,
 *          1,
 */
int rb_right_rotate(struct rb_tree *p_rb_t,struct rb_node *p_n){
    struct rb_node *nil,*p_ori_lchild;
    nil = &p_rb_t->nil;
    if(p_rb_t == NULL || p_n == NULL){
        return -1;
    }
    /*如果需要右旋的节点的左子节点是哨兵节点，则无法旋转*/
    if(p_n->lchild == nil){
        return -1;
    }
    p_ori_lchild = p_n->lchild;  //p_ori_lchild指向需要旋转节点的左子女
    p_n->lchild = p_ori_lchild->rchild;
    if(p_ori_lchild->rchild != nil){
        p_ori_lchild->rchild->parent = p_n; //不改变站哨节点的指针 
    }
    p_ori_lchild->parent = p_n->parent;
    if(p_n->parent == nil){ //如果是根节点
        p_rb_t->root = p_ori_lchild;
    }else if(p_n == p_n->parent->rchild){
        p_n->parent->rchild = p_ori_lchild;
    }else{
        p_n->parent->lchild = p_ori_lchild;
    }
    p_ori_lchild->rchild = p_n;
    p_n->parent = p_ori_lchild;
    return 1;
}

/* 函数名: int rb_insert_fixup(struct rb_tree *p_rb_t,struct rb_node *p_n)
 * 功能: 从节点p_n开始向上修复红黑树的性质
 * 参数: struct rb_tree *p_rb_t,红黑树结构体的指针
 *       struct rb_node *p_n,向上修复红黑树性质的开始节点
 * 返回值:
 */
int rb_insert_fixup(struct rb_tree *p_rb_t,struct rb_node *p_n){
    struct rb_node *p_bad_n; //指向破坏红黑树性质的红色节点的指针，以下简称坏节点
    struct rb_node *p_uncle_n; //指向叔节点
    if(p_rb_t == NULL || p_n == NULL){
        return -1;
    }
    p_bad_n = p_n;
    /*直到破坏红黑树性质的红色节点的父节点为黑色才停止循环，此刻二叉树即为合法红黑树*/
    while(p_bad_n->parent->color == RB_RED){ 
        if(p_bad_n->parent == p_bad_n->parent->parent->lchild){
            /*坏节点的父节点是祖父节点的左子女*/
            p_uncle_n = p_bad_n->parent->parent->rchild; //p_uncle_n指向叔叔节点
            if(p_uncle_n->color == RB_RED){ 
                /* 叔叔节点为红色，即为情况1，此时祖父节点必为黑色，所以对换祖父
                 * 与父节点和叔节点的颜色，最后把祖父节点设为破坏红黑树性质的节点
                 * ，继续向上修复红黑树的性质。
                 */
                p_bad_n->parent->color = RB_BLACK;
                p_uncle_n->color = RB_BLACK;
                p_bad_n->parent->parent->color = RB_RED;
                p_bad_n = p_bad_n->parent->parent; //此时祖父节点为可能的坏节点
                continue;
            }else if(p_bad_n == p_bad_n->parent->rchild){
                /* 如果叔叔不是红色，而且坏节点为其父节点的右子女，此为情况二，需
                 * 对坏节点的父节点进行左旋，进入了情况三。
                 */
                p_bad_n = p_bad_n->parent;
                rb_left_rotate(p_rb_t,p_bad_n);
            }
            /* 如果叔叔不是红色，而且坏节点为其父节点的左子女，此为情况三，对祖父
             * 节点和父亲节点的颜色进行对换之后，在对祖父节点进行右旋，此刻的红黑
             * 树即为合法的红黑树。
             */
            p_bad_n->parent->color = RB_BLACK;
            p_bad_n->parent->parent->color = RB_RED;
            rb_right_rotate(p_rb_t,p_bad_n->parent->parent); //右旋
        }else{ //坏节点的父节点是祖父节点的右子女，对称于上面的操作
            p_uncle_n = p_bad_n->parent->parent->lchild;
            if(p_uncle_n->color == RB_RED){
                p_bad_n->parent->color = RB_BLACK;
                p_uncle_n->color = RB_BLACK;
                p_bad_n->parent->parent->color = RB_RED;
                p_bad_n = p_bad_n->parent->parent; //此时祖父节点为可能的坏节点
                continue;
            }else if(p_bad_n == p_bad_n->parent->lchild){
                p_bad_n = p_bad_n->parent;
                rb_right_rotate(p_rb_t,p_bad_n);
            }
            p_bad_n->parent->color = RB_BLACK;
            p_bad_n->parent->parent->color = RB_RED;
            rb_left_rotate(p_rb_t,p_bad_n->parent->parent); //左旋
        }
    }
    p_rb_t->root->color = RB_BLACK; //确保满足性质2
    return 1;
}

/* 函数名: int rb_insert(struct rb_tree *rb_t,struct rb_node *new_rb_n)
 * 功能: 插入新节点,新插入的节点着为红色，其插入可能破坏了红黑树的性质2和性质4,即
 *       可能破坏了根节点是黑色的和如果一个节点是红色的则它的两个子节点是黑色的性
 *       质，所以在插入之后得要对红黑树进行修复。
 * 参数: struct rb_tree *rb_t,红黑树结构体指针
 *       struct rb_node *new_rb_n,插入的新节点
 * 返回值: -1,
 *          1,
 */
int rb_insert(struct rb_tree *p_rb_t,struct rb_node *pnew_rb_n){
    struct rb_node *pre_node;  //记录刚遍历过的节点
    struct rb_node *p,*nil;  
    if(p_rb_t == NULL || pnew_rb_n == NULL){
        return -1;
    }
    nil = &p_rb_t->nil;
    pre_node = nil;
    p = p_rb_t->root;
    while(p != nil){
        pre_node = p;
        if(pnew_rb_n->key < p->key){
            p = p->lchild;
        }else if(pnew_rb_n->key > p->key){
            p = p->rchild;
        }else{
            return -1;  //存在该节点，插入错误
        }
    }
    pnew_rb_n->parent = pre_node;
    if(pre_node == nil){
        p_rb_t->root = pnew_rb_n;
    }else if(pnew_rb_n->key < pre_node->key){
        pre_node->lchild = pnew_rb_n;
    }else{
        pre_node->rchild = pnew_rb_n;
    }
    pnew_rb_n->rchild = nil;
    pnew_rb_n->lchild = nil;
    pnew_rb_n->color = RB_RED; //为新插入的节点着上红色
    rb_insert_fixup(p_rb_t,pnew_rb_n); //向上修复红黑树的性质
    return 1;
}

/* 函数名: void rb_transplant(struct rb_tree *p_rb_t,struct rb_node *p_ori_n,struct rb_node *p_tr_n)
 * 功能: 把节点p_tr_n移植到p_ori_n所在的位置，也即p_tr_n父节点变为p_ori_n父节点，
 *       p_tr_n变为p_ori_n父节点的子节点，而p_ori_n的子节点不为p_tr_n的子节点，换
 *       句话说，节点p_ori_n的父节点换了个儿子为p_tr_n，包括儿孙。
 * 参数: struct rb_tree *p_rb_t,红黑树结构体指针
 *       strcut rb_node *p_ori_n,point to original node 被代替的节点指针
 *       struct rb_node *p_tr_n,移植的节点的指针
 * 返回值: 无
 *          
 */
void rb_transplant(struct rb_tree *p_rb_t,struct rb_node *p_ori_n,struct rb_node *p_tr_n){
    if(p_ori_n == &p_rb_t->nil || p_ori_n == p_rb_t->root){
        p_rb_t->root = p_tr_n;
    }else if(p_ori_n == p_ori_n->parent->lchild){
        p_ori_n->parent->lchild = p_tr_n;
    }else{
        p_ori_n->parent->rchild = p_tr_n;
    }
    p_tr_n->parent = p_ori_n->parent;
}


/* 函数名: struct rb_node* rb_tree_minimum(struct rb_tree *p_rb_t,struct rb_node *p_root)
 * 功能: 寻找根为p_root的红黑树子树的最小节点
 * 参数: strcut rb_tree *p_rb_t,红黑树结构体指针
 *       strcut rb_node *p_root,指向红黑树的根节点
 * 返回值: NULL,查找失败
 *        !NULL,最小节点的首地址
 */
struct rb_node* rb_tree_minimum(struct rb_tree *p_rb_t,struct rb_node *p_root){
    struct rb_node *p; //用来遍历的节点指针
    struct rb_node *nil;
    /*其实这里可以做一些非法节点的判断，但是为了效率，就交给了调用该函数的函数*/
    nil = &p_rb_t->nil;
    p = p_root;
    while(p->lchild != nil){
        p = p->lchild;
    }
    return p;
}


/* 函数名: int rb_delete_fixup(struct rb_tree *p_rb_t,struct rb_node *p_n)
 * 功能: 做完节点删除操作之后，从节点p_n开始向上修复红黑树的性质
 * 参数: struct rb_tree *p_rb_t,需要修复性质的红黑树的结构体
 *       struct rb_node *p_n,向上修复红黑树性质的开始节点
 * 返回值: -1,修复过程出现了错误
 *          1,修复成功
 */
int rb_delete_fixup(struct rb_tree *p_rb_t,struct rb_node *p_n){
    struct rb_node *p_bad_n;  //一直指向破坏红黑树性质1的双重色节点
    struct rb_node *p_bro_n;  //指向坏节点的兄弟节点
    p_bad_n = p_n;
    /*循环，直到p_bad_n为根或者p_bad_n为红黑节点才停止*/
    while(p_bad_n != p_rb_t->root && p_bad_n->color == RB_BLACK){
        if(p_bad_n == p_bad_n->parent->lchild){ //如果坏节点是左子女
            p_bro_n = p_bad_n->parent->rchild; //则其兄弟节点是右子女
            if(p_bro_n->color == RB_RED){
                /*当兄弟节点是红色的时候，为情况1，此时父亲节点必为黑色*/
                /*交换兄弟节点和父亲节点的颜色，然后对父亲节点进行左旋*/
                p_bro_n->color = RB_BLACK;
                p_bad_n->parent->color = RB_RED;
                rb_left_rotate(p_rb_t,p_bad_n->parent);
                p_bro_n = p_bad_n->parent->rchild;//新的坏节点的兄弟节点
            }
            /* 在此，坏节点的兄弟节点必为黑色，如果上面的if条件成立，则父节点必为
             * 红色，以下针对兄弟节点的子女情况进行分类讨论 */
            if(p_bro_n->lchild->color == RB_BLACK && p_bro_n->rchild->color == RB_BLACK){
                /* 当兄弟节点两子女都为黑色的时候，兄弟节点着为红色，坏节点多一重
                 * 的黑色转移到父节点上，如果多一重的黑色转移到父节点上后父节点是
                 * 红黑节点，则退出循环，如果是双重黑则继续下一个循环。*/
                p_bro_n->color = RB_RED;
                p_bad_n = p_bad_n->parent;//这句就相当于把多一重黑色转移给父节点
                continue;
            }else if(p_bro_n->rchild->color == RB_BLACK){
                /* 如果兄弟节点的左子女是红色而右子女是黑色，交换兄弟节点和其左子
                 * 女的颜色，然后对兄弟节点进行右旋，使得兄弟节点的右子女是红色*/
                p_bro_n->color = RB_RED;
                p_bro_n->lchild->color = RB_BLACK;
                rb_right_rotate(p_rb_t,p_bro_n); //对兄弟节点进行右旋
                p_bro_n = p_bad_n->parent->rchild; //新的兄弟节点
            }
            /* 到此兄弟节点的右子女必为红色，交换兄弟节点和其右子女和父节点的颜色
             * 然后对父节点进行左旋，做完这些操作之后，如果父节点不是根节点，则该
             * 树必为合法红黑树。 */
            p_bro_n->color = p_bad_n->parent->color;
            p_bad_n->parent->color = RB_BLACK; //把坏节点的多一重黑色转移给父节点
            p_bro_n->rchild->color = RB_BLACK; //把兄弟节点的黑色下放给其右子女
            rb_left_rotate(p_rb_t,p_bad_n->parent); //对父节点进行左旋
            p_bad_n = p_rb_t->root;  //此时坏节点可能是根节点了
        }else{//如果坏节点是右子女,操作是对称的
            p_bro_n = p_bad_n->parent->lchild; 
            if(p_bro_n->color == RB_RED){
                p_bro_n->color = RB_BLACK;
                p_bad_n->parent->color = RB_RED;
                rb_right_rotate(p_rb_t,p_bad_n->parent);
                p_bro_n = p_bad_n->parent->lchild;//新的坏节点的兄弟节点
            }
            if(p_bro_n->rchild->color == RB_BLACK && p_bro_n->lchild->color == RB_BLACK){
                p_bro_n->color = RB_RED;
                p_bad_n = p_bad_n->parent;//这句就相当于把多一重黑色转移给父节点
                continue;
            }else if(p_bro_n->lchild->color == RB_BLACK){
                p_bro_n->color = RB_RED;
                p_bro_n->rchild->color = RB_BLACK;
                rb_left_rotate(p_rb_t,p_bro_n); 
                p_bro_n = p_bad_n->parent->lchild; 
            }
            p_bro_n->color = p_bad_n->parent->color;
            p_bad_n->parent->color = RB_BLACK; 
            p_bro_n->lchild->color = RB_BLACK; 
            rb_right_rotate(p_rb_t,p_bad_n->parent); 
            p_bad_n = p_rb_t->root;  
        }
    }
    /* 因为p_bad_n为红黑色时，color属性为RED的，所以得着为单黑色。而如果p_bad_n
     * 为根且为双重黑，下面执行的语句也意味着把双重黑着为单重黑。*/
    p_bad_n->color = RB_BLACK; 
    return 1;
}


/* 函数名: int rb_delete(struct rb_tree *p_rb_t,struct rb_node *p_rm_n)
 * 功能: 删除红黑树中的一个节点，注意，只是从红黑树移除该节点，而没有释放节点的内
 *       存。
 * 参数: struct rb_tree *p_rm_t,红黑树结构体指针
 *       struct rb_node *p_rb_n,需要删除的节点。注意，只是把节点从红黑树中移除
 * 返回值: -1,
 *          1,
 */
int rb_delete(struct rb_tree *p_rb_t,struct rb_node *p_rm_n){
    struct rb_node *p_mv_n;  //一直指向需要移动的节点
    struct rb_node *p_replace_n;  //将要替代被移动节点的节点
    struct rb_node *nil;
    unsigned char ori_color; //记录需要移动的节点的颜色
    if(p_rb_t == NULL || p_rm_n == NULL){
        return -1;
    }
    nil = &p_rb_t->nil;
    p_mv_n = p_rm_n;  //最初需要移动的节点就是需要删除的节点
    ori_color = p_mv_n->color; //记录着移动节点的颜色
    if(p_rm_n->lchild == nil){  
        /*如果左子女节点是空，则由右子女来替代父亲节点的位置*/
        p_replace_n = p_rm_n->rchild;
        rb_transplant(p_rb_t,p_rm_n,p_rm_n->rchild);
    }else if(p_rm_n->rchild == nil){
        /*如果右子女节点是空，则由左子女来替代父亲节点的位置*/
        p_replace_n = p_rm_n->lchild;
        rb_transplant(p_rb_t,p_rm_n,p_rm_n->lchild);
    }else{
        /* 如果将要被删除的节点的左右子女都不是空的，则需要找右子树中的最小节点来
         * 代替被删除节点。这样问题转变为右子树中最小节点被移走的问题。
         */
        p_mv_n = rb_tree_minimum(p_rb_t,p_rm_n->rchild);
        ori_color = p_mv_n->color; //记录将要移走节点的颜色
        p_replace_n = p_mv_n->rchild; //被移走的节点位置将要被其右子女继承
        if(p_mv_n->parent == p_rm_n){ 
            /*如果将要被移动的节点恰好是要被删除节点的右子女*/
            p_replace_n->parent = p_mv_n;
        }else{
            /* 否则就让将要被移动的节点移动为将要被删除的节点的右子女，被移动的节
             * 点的右子女继承被移动节点的位置，但不继承颜色。
             */
            rb_transplant(p_rb_t,p_mv_n,p_mv_n->rchild);//右子女继承父亲的位置
            /* 最小节点移动为被删除节点的右子女，这个过程可以看《算法导论》167页
             * 图d
             */
            p_mv_n->rchild = p_rm_n->rchild;
            p_mv_n->rchild->parent = p_mv_n;
        }
        /*最小节点继承被删除节点的位置和颜色*/
        rb_transplant(p_rb_t,p_rm_n,p_mv_n); 
        p_mv_n->lchild = p_rm_n->lchild;
        p_mv_n->lchild->parent = p_mv_n;
        p_mv_n->color = p_rm_n->color;
    }
    if(ori_color == RB_BLACK){
        /*如果移动的节点的颜色为黑色，则在移动之后很有可能破坏了红黑树的性质*/
        rb_delete_fixup(p_rb_t,p_replace_n); //向上修复红黑树性质
    }
    return 1;
}



/* 函数名: int rb_pre_traversal(struct rb_tree *p_rb_t,struct rb_node *p_root)
 * 功能: 前序遍历红黑树
 * 参数: struct rb_tree *p_rb_t,指向红黑树结构体的指针
 *       struct rb_node *p_root,红黑树的跟节点
 * 返回值: -1,
 *          1,
 */
int rb_pre_traversal(struct rb_tree *p_rb_t,struct rb_node *p_root){
    if(p_rb_t == NULL || p_root == NULL){
        return -1;
    }
    if(p_root == &p_rb_t->nil){
        return 1;
    }
    printf("%d:%s",p_root->key,p_root->color == 1?"B":"R");
    printf("(");
    if(rb_pre_traversal(p_rb_t,p_root->lchild) == -1){
        return -1;
    }
    printf(")");
    printf("(");
    if(rb_pre_traversal(p_rb_t,p_root->rchild) == -1){
        return -1;
    }
    printf(")");
    return 1;
}
