#ifndef _M_BTREE_H
#define _M_BTREE_H

#ifndef _GNU_SOURCE
#define _GNU_SOURCE
#endif

#include <stdbool.h>
#include <pthread.h>
#include "../memory_pool/memory_pool.h"
#define STRING_SIZE  21


#ifdef __cplusplus
extern "C" {
#endif

typedef struct s_data_s {

    int a;
    int b;
    
} s_data_t;

typedef struct {
    char key[STRING_SIZE];
    void *data_ptrs;
} bkey_t;

typedef struct btree_node_s {

    // 这里用int来作为键值
    bkey_t *keys;
    // int *keys;
    struct btree_node_s **children;
    int num_keys;
    bool is_leaf;

} btree_node_t;


typedef struct btree_s {

    btree_node_t *root;
    // 当前节点下子节点的最少数量,确定了每个节点的最小键宽度
    int t;
    int max_key;
    int min_key;
    pthread_rwlock_t rwlock;
} btree_t;

typedef struct search_result_s {

    btree_node_t *node;// 键所在的节点指针
    int index;// 键在当前节点中的索引
    bool found;// 是否找到
    
} search_result_t;



extern btree_t* btree_create(int);
// tree, key
extern bool btree_insert(btree_t*, bkey_t, fixed_size_pool_t*);
extern bool btree_contains(btree_t*, bkey_t);
extern search_result_t btree_search(btree_t *, bkey_t);
extern bool btree_remove(btree_t*, bkey_t, fixed_size_pool_t*);
extern void btree_inorder_traversal(btree_t*);
extern void btree_inorder_traversal_test_array(btree_t*, int*);
extern fixed_size_pool_t *int_global_fixed_pool;
extern btree_t *global_m_btree;
#ifdef __cplusplus
}
#endif
#endif