#include "m_btree.h"

#include <stdlib.h>
#include <string.h>
#include <stdio.h>

static btree_node_t* create_node(bool is_leaf, int t) {

    btree_node_t *node = (btree_node_t*)malloc(sizeof(btree_node_t));
    if (!node) {
        printf("malloc node failed \n" );
        exit(-1);
    }

    // 初始化键数组,data数组,child数组
    node->keys = (int*)malloc(sizeof(int) * (2*t - 1));
    if (!node->keys) {
        printf("malloc keys failed\n");
        exit(-1);
    }
    memset(node->keys, 0, sizeof(int) * (2*t - 1));
    node->data_ptrs = malloc(sizeof(void*) * (2*t - 1));
    if (!node->data_ptrs) {
        printf("malloc sdata failed\n");
        exit(-1);
    }
    memset(node->data_ptrs, 0, sizeof(void*) * (2*t - 1));
    node->children = (btree_node_t*)malloc(sizeof(btree_node_t*) * 2 * t);
    if (!node->children) {
        printf("malloc child node failed\n");
        exit(-1);
    }
    memset(node->children, 0, sizeof(btree_node_t*) * (2 * t));

    node->is_leaf = is_leaf;
    node->num_keys = 0;
    return node;
}

btree_t* btree_create(int t) {

    btree_t *btree = (btree_t*)malloc(sizeof(btree_t));
    if (!btree) {

        printf("malloc btree failed\n");
        exit(-1);
    }
    btree->root = create_node(true, t);
    if (!btree->root) {
        pritnf("create failed\n");
        exit(-1);
    }

    btree->t = t;
    btree->max_key = 2*t - 1;
    btree->min_key = t - 1;
    return btree;    
}

// 调用者保证叶子节点未满
static bool insert_into_leaf(btree_node_t *node, int key) {
    assert(node->is_leaf);
    
    int insert_index = node->num_keys - 1;
    while(insert_index >= 0 && key < node->keys[insert_index]) {

        node->keys[insert_index + 1] = node->keys[insert_index]; 
        insert_index--;
    }
    
    node->keys[insert_index + 1] = key;
    node->num_keys++;
    return true;
}


static bool split_child(btree_node_t *parent, int index, btree_node_t *child, int t) {

// 这里不需要判断parent是否是满，因为insert保证了路径上的节点如果满都会被split
// 这里调用者保证，当前child一定满，才会调用split_child
    // 创建新节点

    btree_node_t *new_child = malloc(sizeof(btree_node_t));
    if (!new_child) {
        printf("malloc new_child failed\n");
        exit(-2);
    }

    // 把原child的key和children按分裂逻辑放到new_child
    // key从t位置开始拿，拿t - 1个，因为当前有2t-1,那么每个子节点有t-1个，中间1个向上传递
    for (int i = 0; i < t - 1; i++) {
        new_child->keys[i] = child->keys[t + i];
    }
    // 把child 的children放到new_child中
    // 中间位置t - 1,对应中间键的前驱区间，所以还是从t位置开始拷贝
    // 拷贝到new_child 

    if (!child->is_leaf) {
        for (int i = 0; i < t; i++) {
            new_child->children[i] = child->children[t + i];
        }
    }
    new_child->num_keys = t - 1;
    child->num_keys = t - 1;

    // 如果Parent节点0个节点，那么相当于把index位置的key直接插入
    // 更改index + 1位置的children
    // process parent
    // move keys ,from index to last
    for (int i = parent->num_keys - 1; i >= index; i--) {
        parent->keys[i + 1] = parent->keys[i];
    }
    parent->keys[index] = child->keys[t - 1];
    
    //move parent`s children
    // 这里index的前驱children不动，index + 1才是新插入的new_child插入位置
    // 这里parent节点肯定不满，所以可以直接访问num_keys + 1的位置
    for (int i = parent->num_keys; i > index; i--) {
        parent->children[i + 1] =  parent->children[i];
    }
    parent->children[index + 1] = new_child;
    parent->num_keys++;
    return true;    
}


bool btree_insert(btree_t *tree, int key) {

    // 从根节点开始，直到遍历到叶子节点
    // 在叶子节点进行insert_into_leaf，并且保证叶子节点非满
    
    if (!tree) {
        printf("tree null\n");
        exit(-3);
    }
    
    // while遍历，但是split需要上层节点信息，所以从当前节点提前判断要进入的子节点是否需要split
    // 这里从root进去，所以需要先处理root是否需要split
    if (!tree->root) {
        printf("root is NULL\n");
        exit(-3);
    }
    int t = tree->t;
    int max = 2*t - 1;
    btree_node_t *current = tree->root;
    if (current->num_keys == 2 * t - 1) {
        btree_node_t *new_root = create_node(false, t);
        // 让new_root指向当前root
        new_root->children[0] = current;
        split_child(new_root, 0, current, t);
        tree->root = new_root;
        current = new_root;
    }
    
    while(!current->is_leaf) {

        // 不是叶子节点则去当前节点找下一个位置的孩子节点
        // 先遍历当前键要插入的区间

        int index = current->num_keys - 1;
        while(index >= 0 && key < current->keys[index]) index--;
        index ++;
        //需要插入的区间的索引也是index,因为要插入的位置是当前键的左边的区间
        // 前驱一一对应，所以索引也是相同的
        btree_node_t *child = current->children[index];
        // 判断子节点是否需要split
        if (child->num_keys == max) {
            split_child(current, index, child, t);
            // 当前节点新增子节点到当前，要插入的子节点分成2个
            // 判断要去到第一个还是第二个,大于拿上来的键则去到第二个
            if (key > current->keys[index]) index++;
            child = current->children[index];
        }
        current = child;
    }
    // 叶子节点，且数据不可能最多                                                                                                             
    insert_into_leaf(current, key);
    return true;    
}



