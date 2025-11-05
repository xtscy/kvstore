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

bool btree_contains(btree_t *tree, int key) {

    if (!tree || !tree->root) {
        perror("tree is unfair\n");
        return false;
    }

    btree_node_t *current = tree->root;
    while (current) {
        int i = 0;
        while (i < current->num_keys && key > current->keys[i]) i++;
        if (i < current->num_keys && key == current->keys[i]) {
            return true;
        }

        //当前节点没找到,可能i==num_keys或者key < keys[i]
        // 先判断是否是叶子节点
        if (current->is_leaf) {
            return false;
        }
        // 这里如果大于最后一个键，说明去到键的右区间
        // 如果小于i位置的键，说明去到左区间
        // if (i == current->num_keys) {
        //     current = current->children[i];
        // } else {
        //     current = current->children[i];
        // }
        current = current->children[i];
    }
    
    return false;
    
}

search_result_t btree_search(btree_t *tree, int key) {

    // 和上面逻辑大致相同，找到键后返回result_t
    search_result_t ret = {
        .found = false,
        .index = -1,
        .node = NULL,
    };

    if (!tree || !tree->root) {
        return ret;
    }
    btree_node_t* current = tree->root;
    while (current) {

        int index = 0;
        while (index < current->num_keys && current->keys[index] < key) index++;
        if (index < current->num_keys && current->keys[index] == key) {
            ret.found = true;
            ret.index = index;
            ret.node = current;
            return ret;
        }

        if (current->is_leaf) {
            // 这里可以返回最后一个访问的节点
            // 由于没有找到，就可以返回当前值在当前节点中应该插入的位置
            ret.found = false;
            ret.index = index;
            ret.node = current;
            return ret;
        }
        
        current = current->children[index];
    }

    return ret;
}

// 删除操作
// 该操作的核心逻辑是不破坏B树的结构性质

// 当子树键不满足最小删除键要求时需要使用填充键
// 填充键，使用借位的方式，借当前父节点的左兄弟或者右兄弟
// 这里借位如果左右兄弟借不了，还是使用合并节点和键的方式
// 把当前键下放，因为我所到达的父节点在到达之前肯定会进行填充检查
// 确保该节点能够满足删除的最低键个数要求

// 当满足了填充键个数时就可以查找是否在当前节点删除
// 删除的话就使用下面3种方式
// 使用递归前驱或后继(键个数大于等于t)，或者使用合并子树和当前键的方式(如果前面不行)
// 来进行删除

// 这里需要实现
// 借左兄弟，借右兄弟，递归删除前驱或者后继(可以使用循环代替递归)，合并节点和键
// 然后就是填充操作，调用上面的子功能
// 最后就是整体的删除函数(传递树节点指针)
// 这里删除函数可以外层再包一层函数传递树指针

// 这里再封装一个查询index在当前节点的函数

// bool is_secure(btree_node_t *node) {

//     if (!node) {
//         return fasle;
//     }
//     return true;    
// }

// 没有重复键
static int find_key_idx(btree_node_t *node, int key) {

    if (!node) {
        printf("node is null at find_key_idx\n");
        exit(-2);
    }
    int idx = 0;
    for (idx = 0; idx < node->num_keys; idx++) {

        if (node->keys[idx] <= key) {
            return idx;
        }
    }
    return node->num_keys;
}

// node是当前所在节点，idx是当前要删除的键的索引位置
// 这里找前驱然后返回前驱，然后在外部函数中处理
static int get_precursor(btree_node_t *node, int idx) {

    if (!node) {
        pritnf("node is null at get_successor\n");
        exit(-2);
    }
    btree_node_t *current = node->children[idx];
    while (!current->is_leaf) {
        current = current->children[current->num_keys];
    }
    return current->keys[current->num_keys - 1];
}

// 拿到后继的最小值节点
static int get_successor(btree_node_t *node, int idx) {

    if (!node) {
        printf("node is null at get_successor\n");
        exit(-2);
    }
    btree_node_t *current = node->children[idx + 1];
    while (!current->is_leaf) {
        current = current->children[0];
    }
    return current->keys[0];    
}


// 这里合并的前提是
// 左右子节点键的个数都是MIN_KEYS
// 这里不可能小于最小值，因为不符合B树性质，插入时split键最少都是t-1最小键个数
// 也不可能大于最小值，否则就会调用递归前驱或者后继,满足键个数要求
// 这里合并idx左右两个子区间
// 即idx 和 idx + 1
// 把值合并到左子节点，然后覆盖丢弃right_child

static bool destroy_node(btree_node_t *node) {

    if (!node) {
        pritnf("node is null at destroy_node\n");
        exit(-2);
    }
    
    free(node->keys);
    free(node);
    return true;
}


static bool merge_node(btree_node_t *parent, int idx) {

    if (!parent) {

        printf("parent is null at merge_node\n");
        exit(-2);
    }
    btree_node_t *left_child = parent->children[idx];
    btree_node_t *right_child = parent->children[idx + 1];

    // 把key全部搬到left_child
    left_child->keys[left_child->num_keys] = parent->keys[idx];
    for (int i = 0; i < right_child->num_keys; i++) {
        left_child->keys[left_child->num_keys + 1 + i] = right_child->keys[i];
    }
    // 把右子树的节点全部搬到左子树的children
    for (int i = 0; i <= right_child->num_keys; i++) {
        left_child->children[left_child->num_keys + 1 + i] = right_child->children[i];
    }
    // 把父节点的键和右子树的键所对应的data_ptrs全部搬到左子树
    // 因为键都搬到了左子树

    // 覆盖parent节点的键

    for (int i = idx + 1; i < parent->num_keys; i++) {
        parent->keys[i - 1] = parent->keys[i];
    }
    // 覆盖parent节点的指针
    for (int i = idx + 2; i <= parent->num_keys; i++) {
        parent->children[i - 1] = parent->children[i];
    }
    parent->num_keys --;
    right_child->is_leaf = true;
    destroy_node(right_child);
    return true;
}

// 借位


// 借位前会判断是否满足借位要求
// 这里会保证左兄弟有足够的键，以此来填充右兄弟的键个数

// 这里idx是当前要填充的子区间的索引
static borrow_from_left(btree_node_t *parent, int idx) {

    if (!parent) {
        printf("parent is null at borrow_from_left\n");
        exit(-2);
    }

    btree_node_t *left_node = parent->children[idx - 1];
    btree_node_t *right_node = parent->children[idx];

// 把左兄弟的最大键放到父节点，父节点下移到右子节点
// 这里必然有位置，不会有溢出问题
    int p_key = parent->keys[idx - 1];
    // move key
    for (int i = right_node->num_keys; i > 0; i--) {
        right_node->keys[i] = right_node->keys[i - 1];
    }
// move children
    if (!left_node->is_leaf) {

        btree_node_t *left_last = left_node->children[left_node->num_keys];
        for (int i = right_node->num_keys + 1; i > 0; i--) {
            right_node->children[i] = right_node->children[i - 1];
        }
        right_node->children[0] = left_last;
    }
// exchange parent key and child key    
    right_node->keys[0] = p_key;
    int left_key = left_node->keys[left_node->num_keys - 1];
    parent->keys[idx - 1] = left_key;
    left_node->num_keys--;
    right_node->num_keys++;
}

static void borrow_from_right(btree_node_t *parent, int idx) {

    if (!parent) {
        printf("parent is null at borrow_from_right\n");
        exit(-2);
    }
    btree_node_t *left_node = parent->children[idx];
    btree_node_t *right_node = parent->children[idx + 1];
// exchange key and children
    left_node->keys[left_node->num_keys] = parent->keys[idx];
    parent->keys[idx] = right_node->keys[0];
    if (!left_node->is_leaf) {
        left_node->children[left_node->num_keys + 1] = right_node->children[0];
    }

//move right child key and children
    for (int i = 1; i < right_node->num_keys; i++) {
        right_node->keys[i - 1] = right_node->keys[i];
    }
    for (int i = 1; i <= right_node->num_keys; i++) {
        right_node->children[i - 1] = right_node->children[i];
    }
    
    left_node->num_keys++;
    right_node->num_keys--;
}


static void remove(btree_node_t *node, int key) {


    
}


bool btree_remove(btree_t *tree, int key) {

    if (!tree || !tree->root) {
        printf("tree is null at btree_remove\n");
        exit(-2);
    }
     
    
    
}   

 