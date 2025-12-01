#ifndef BTREE_H

#define _GNU_SOURCE
#define BTREE_H

#include <stdbool.h>
#include <pthread.h>
#include <stdatomic.h>

typedef struct s_data_s {

    int a;
    int b;
    
} s_data_t;


typedef struct btree_node_s {

    // 这里的加锁逻辑是
    // 锁节点而不是整颗树
    // 每个线程只锁当前占用的节点，最多锁两个节点，即当前节点和某个孩子节点
    // 提高了并发度

    pthread_rwlock_t rw_lock;
    // split status 
    //* atomic_bool being_split;
    //* 这里不需要分裂标志
    //* 这里对于分裂的出的split
    //* 先设置high_key然后设置right_sibling
    //* 最后把新的节点插入进去
    //* 这里插入进去是直接把左兄弟的right_sibling指向当前新节点
    // *但是这里有个问题，如果我要搜素的节点被某个insert操作，导致目标键所在的节点分裂
    //* 这里的关键是原目标节点的high_key和right_sibing多久更新，其实这里键已经在当前节点，那么主要是high_key
    //* 如果在split函数执行完前，并未更改high_key那么就会在当前原节点查找,那么就是split上下文的原节点元信息的更新如何更新的问题
    //* 这里是在更新前设置
    //* 这里主要是修复，如果分裂节点元信息还未更新完成的情况
    //* 如果未更新完成是否会有并发问题

    //? 分裂标志和元信息是否可用
    //? 是否需要两个标志
    //? 这里使用meta
    //? 当更改元信息在最开始就让meta失效
    // atomic_bool being_split;
    atomic_bool meta_valid;

//* 64分页，避免false sharing

    _Atomic(btree_node_t*) right_sibling;
    _Atomic(int) high_key;
    
    int *keys;
    void **data_ptrs;
    struct btree_node_s **children;
    int num_keys;
    bool is_leaf;

} btree_node_t;


typedef struct btree_s {

    //root指针也需要锁，在根分裂时，为了强一致性
    //使其他线程读到的根是最新的
    pthread_rwlock_t rw_lock;
    
    _Atomic(btree_node_t*) root;
    // 当前节点下子节点的最少数量,确定了每个节点的最小键宽度
    int t;
    int max_key;
    int min_key;

} btree_t;

typedef struct search_result_s {

    btree_node_t *node;// 键所在的节点指针
    int index;// 键在当前节点中的索引
    bool found;// 是否找到
    
} search_result_t;


extern btree_t* btree_create(int);
// tree, key
extern bool btree_insert(btree_t*, int);
extern bool btree_contains(btree_t*, int);
extern search_result_t btree_search(btree_t *, int);
extern bool btree_remove(btree_t*, int);
extern void btree_inorder_traversal(btree_t*);
extern void btree_inorder_traversal_test_array(btree_t*, int*);

#endif