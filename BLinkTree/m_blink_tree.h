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
    atomic_bool being_split;
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