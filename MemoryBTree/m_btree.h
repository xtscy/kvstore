#ifndef BTREE_H
#define BTREE_H

#include <stdbool.h>


typedef struct s_data_s {

    int a;
    int b;
    
} s_data_t;


typedef struct btree_node_s {

    // 这里用int来作为键值
    int *keys;
    void **data_ptrs;
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

} btree_t;


extern btree_t* btree_create(int);
extern bool btree_insert(btree_t*, int);


#endif