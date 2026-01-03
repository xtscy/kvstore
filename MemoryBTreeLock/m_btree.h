#ifndef _M_BTREE_H
#define _M_BTREE_H

#ifndef _GNU_SOURCE
#define _GNU_SOURCE
#endif

#include <stdbool.h>
#include <pthread.h>
// #include "../memory_pool/memory_pool.h"
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
    uint16_t length;//todo 改到这了
    //todo 然后需要更改比较函数,这里需要保证二进制安全
    //todo 然后再去更改各个相关函数，插入的时候也需要多一个键的长度值
    //todo 这里值还是先存int,后续再来优化
} bkey_t;

typedef enum btree_ite_states_e {

    ITER_STATE_START,      // 初始状态，需要找到第一个元素
    ITER_STATE_AT_KEY,     // 在当前关键字位置,可以不用
    ITER_STATE_ENTER_CHILD, // 当前节点状态为进入子树
    ITER_STATE_BACKTRACK,  // 表示以从当前节点向上回溯
    ITER_STATE_END         // 迭代结束
    
} btree_ite_states;
typedef struct btree_node_s {

    bkey_t *keys;
    struct btree_node_s **children;
    int num_keys;
    bool is_leaf;
    // 指向父节点
    struct btree_node_s *parent;

    int ite_index;
    btree_ite_states state;
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



typedef struct btree_iterator_s {
    // 
    btree_node_t *current;
    
} btree_iterator_t;

typedef struct fixed_size_pool fixed_size_pool_t;

extern btree_iterator_t* create_iterator(btree_t*);
extern btree_iterator_t* iterator_find_next(btree_iterator_t *iterator);
extern bkey_t iterator_get(btree_iterator_t*); 

extern btree_t* btree_create(int);
// tree, key
extern bool btree_insert(btree_t*, bkey_t, fixed_size_pool_t*);
extern bool btree_contains(btree_t*, bkey_t);
extern search_result_t btree_search(btree_t *, bkey_t*);
extern bool btree_remove(btree_t*, bkey_t, fixed_size_pool_t*);
extern void btree_inorder_traversal(btree_t*);
extern void btree_inorder_traversal_test_array(btree_t*, int*);
extern void* fixed_pool_alloc(fixed_size_pool_t*);
extern void fixed_pool_free(fixed_size_pool_t*, void*);
extern fixed_size_pool_t* fixed_pool_create(size_t block_size, size_t block_count);
extern void fixed_pool_destroy(fixed_size_pool_t*);
extern void fixed_pool_stats(fixed_size_pool_t*);
extern fixed_size_pool_t *int_global_fixed_pool;
extern btree_t *global_m_btree;
#ifdef __cplusplus
}
#endif
#endif