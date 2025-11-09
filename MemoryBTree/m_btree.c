#include "m_btree.h"

#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include <assert.h>

static btree_node_t* create_node(bool is_leaf, int t) {

    btree_node_t* node = (btree_node_t*)malloc(sizeof(btree_node_t));
    if (!node) {
        printf("malloc node failed \n");
        exit(-1);
    }

    node->keys = (int*)malloc(sizeof(int) * (2 * t - 1));
    if (!node->keys) {
        printf("malloc keys failed\n");
        exit(-1);
    }
    memset(node->keys, 0, sizeof(int) * (2 * t - 1));
    node->data_ptrs = malloc(sizeof(void*) * (2 * t - 1));
    if (!node->data_ptrs) {
        printf("malloc sdata failed\n");
        exit(-1);
    }
    memset(node->data_ptrs, 0, sizeof(void*) * (2 * t - 1));
    node->children = (btree_node_t**)malloc(sizeof(btree_node_t*) * 2 * t);
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

    btree_t* btree = (btree_t*)malloc(sizeof(btree_t));
    if (!btree) {

        printf("malloc btree failed\n");
        exit(-1);
    }
    btree->root = create_node(true, t);
    if (!btree->root) {
        printf("create failed\n");
        exit(-1);
    }

    btree->t = t;
    btree->max_key = 2 * t - 1;
    btree->min_key = t - 1;
    return btree;
}

// 调用者保证叶子节点未满
static bool insert_into_leaf(btree_node_t* node, int key) {
    assert(node->is_leaf);

    int insert_index = node->num_keys - 1;
    while (insert_index >= 0 && key < node->keys[insert_index]) {

        node->keys[insert_index + 1] = node->keys[insert_index];
        insert_index--;
    }

    node->keys[insert_index + 1] = key;
    node->num_keys++;
    return true;
}


// 分裂节点，把中间键拿到parent,index是当前键插入到parent的哪个位置
// 所以parent的num_keys > index
// child作为当前的左孩子
static void split_child(btree_node_t* parent, int index, btree_node_t* left_child, int t) {

    // 这里不需要判断parent是否是满，因为insert保证了路径上的节点如果满都会被split
    // 这里调用者保证，当前child一定满，才会调用split_child
        // 创建新节点
    bool is_leaf = left_child->is_leaf;
    btree_node_t* right_child = create_node(is_leaf, t);
    // printf("right_node->is_leaf %d\n", right_child->is_leaf);
    if (!right_child) {
        printf("malloc new_child failed\n");
        exit(-2);
    }

    // 先构建right_child
    // 把left_child的键拿到right_child中

    for (int i = 0; i < t - 1; i++) {
        right_child->keys[i] = left_child->keys[t + i];
    }

    if (!is_leaf) {
        // 不是叶子节点，把left_child的children拿到right_child
        for (int i = 0; i < t; i++) {
            right_child->children[i] = left_child->children[t + i];
        }
    }
    right_child->num_keys = t - 1;
    // 构造完right_child后, 更改parent
    // parent 
    //key
    for (int i = parent->num_keys - 1; i >= index; i--) {
        parent->keys[i + 1] = parent->keys[i];
    }
    parent->keys[index] = left_child->keys[t - 1];
    // parent不可能是叶子节点，外部调用保证，所以这里肯定需要move parent children array
    // children
    for (int i = parent->num_keys + 1; i > index + 1; i--) {
        parent->children[i] = parent->children[i - 1];
    }
    parent->children[index + 1] = right_child;
    parent->num_keys++;
    // parent的children更改完成

    // 最后更改左子节点的个数
    left_child->num_keys = t - 1;
    //
}


bool btree_insert(btree_t* tree, int key) {

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
    // printf("----------------1.2-----------------\n");
    int t = tree->t;
    int max = 2 * t - 1;
    btree_node_t* current = tree->root;
    // 这里判断根节点是否满，如果满把中间键上提到新的root节点
    if (current->num_keys == 2 * t - 1) {
        // printf("------------1.3-----------------\n");
        btree_node_t* new_root = create_node(false, t);
        // printf("up malloc\n");
        // 让new_root指向当前root
        new_root->children[0] = current;
        // printf("-----------split child------------\n");
        split_child(new_root, 0, current, t);
        // printf("------------split child end----------------\n");
        tree->root = new_root;
        current = new_root;
        // printf("------------1.3end-----------\n");
    }

    // printf("current->is_leaf%d, current->num_keys%d, current->keys[0]:%d\n",
        // current->is_leaf, current->num_keys, current->keys[0]);

    while (!current->is_leaf) {

        // 不是叶子节点则去当前节点找下一个位置的孩子节点
        // 先遍历当前键要插入的区间
        // printf("----------------1.4---------------\n");

        // int index = current->num_keys - 1;
        // while(index >= 0 && key < current->keys[index]) index--;
        // index ++;
        int index = 0;
        // printf("begin run\n");
        while (index < current->num_keys && key > current->keys[index]) index++;
        // printf("run end\n");

        //需要插入的区间的索引也是index,因为要插入的位置是当前键的左边的区间
        // 前驱一一对应，所以索引也是相同的
        btree_node_t* child = current->children[index];
        // printf("index:%d\n", index);
        if (!child) {
            printf("child is NULL\n");
            exit(-2);
        }
        // else {
        //     // printf("begin printf child\n");
        //     // printf("child->num_keys:%d\n", child->num_keys);
        //     // printf("child->keys[0]:%d\n", child->keys[0]);
        //     // printf("child->key:%d, child->num_keys:%d\n", child->keys[0], child->num_keys);
        //     // printf("printf child end\n");
        // }
        // 判断子节点是否需要split
        if (child->num_keys == max) {
            split_child(current, index, child, t);
            // 当前节点新增子节点到当前，要插入的子节点分成2个
            // 判断要去到第一个还是第二个,大于拿上来的键则去到第二个
            if (key > current->keys[index]) {
                index++;
                // printf("index ++\n");
            }
            child = current->children[index];
        }
        // printf("key:%d, index:%d\n", key, index);
        current = child;
        // printf("current->is_leaf:%d\n", current->is_leaf);
        // printf("----------------1.5---------------\n");
    }
    // 叶子节点，且数据不可能最多
    insert_into_leaf(current, key);
    return true;
}

bool btree_contains(btree_t* tree, int key) {

    if (!tree || !tree->root) {
        perror("tree is unfair\n");
        return false;
    }

    btree_node_t* current = tree->root;
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

search_result_t btree_search(btree_t* tree, int key) {

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
static int find_key_idx(btree_node_t* node, int key) {

    if (!node) {
        printf("node is null at find_key_idx\n");
        exit(-2);
    }
    int idx = 0;
    for (idx = 0; idx < node->num_keys; idx++) {

        if (key <= node->keys[idx]) {
            return idx;
        }
    }
    return node->num_keys;
}

// node是当前所在节点，idx是当前要删除的键的索引位置
// 这里找前驱然后返回前驱，然后在外部函数中处理
static int get_precursor(btree_node_t* node, int idx) {

    if (!node) {
        printf("node is null at get_successor\n");
        exit(-2);
    }
    btree_node_t* current = node->children[idx];
    while (!current->is_leaf) {
        current = current->children[current->num_keys];
    }
    return current->keys[current->num_keys - 1];
}

// 拿到后继的最小值节点
static int get_successor(btree_node_t* node, int idx) {

    if (!node) {
        printf("node is null at get_successor\n");
        exit(-2);
    }
    btree_node_t* current = node->children[idx + 1];
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

static bool destroy_node(btree_node_t* node) {

    if (!node) {
        printf("node is null at destroy_node\n");
        exit(-2);
    }

    free(node->keys);
    free(node);
    return true;
}

//idx为要合并的左子区间节点索引
static void merge_node(btree_node_t* parent, int idx) {

    if (!parent) {

        printf("parent is null at merge_node\n");
        exit(-2);
    }
    btree_node_t* left_child = parent->children[idx];
    btree_node_t* right_child = parent->children[idx + 1];

    // 把key全部搬到left_child
    left_child->keys[left_child->num_keys] = parent->keys[idx];
    for (int i = 0; i < right_child->num_keys; i++) {
        left_child->keys[left_child->num_keys + 1 + i] = right_child->keys[i];
    }
    // 把右子树的children全部搬到左子树
    for (int i = 0; i <= right_child->num_keys; i++) {
        left_child->children[left_child->num_keys + 1 + i] = right_child->children[i];
    }
    left_child->num_keys += right_child->num_keys + 1;
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
    parent->num_keys--;
    destroy_node(right_child);
}

// 借位


// 借位前会判断是否满足借位要求
// 这里会保证左兄弟有足够的键，以此来填充右兄弟的键个数

// 这里idx是当前要填充的子区间的索引
static void borrow_from_left(btree_node_t* parent, int idx) {

    if (!parent) {
        printf("parent is null at borrow_from_left\n");
        exit(-2);
    }

    btree_node_t* left_node = parent->children[idx - 1];
    btree_node_t* right_node = parent->children[idx];

    // 把左兄弟的最大键放到父节点，父节点下移到右子节点
    // 这里必然有位置，不会有溢出问题
    int p_key = parent->keys[idx - 1];
    // move key
    for (int i = right_node->num_keys; i > 0; i--) {
        right_node->keys[i] = right_node->keys[i - 1];
    }
    // move children
    if (!left_node->is_leaf) {

        btree_node_t* left_last = left_node->children[left_node->num_keys];
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
// idx指向当前区间
static void borrow_from_right(btree_node_t* parent, int idx) {

    if (!parent) {
        printf("parent is null at borrow_from_right\n");
        exit(-2);
    }
    btree_node_t* left_node = parent->children[idx];
    btree_node_t* right_node = parent->children[idx + 1];
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
    if (!right_node->is_leaf) {
        for (int i = 1; i <= right_node->num_keys; i++) {
            right_node->children[i - 1] = right_node->children[i];
        }
    }
    

    left_node->num_keys++;
    right_node->num_keys--;
}

// idx为要填充的区间的索引
static btree_node_t* fill_node(btree_node_t* parent, int idx, int t) {

    btree_node_t* target = parent->children[idx];
    // 这里idx在num_keys和小于num_keys
    // 这里先借左兄弟和右兄弟，如果不能借，则用下沉来填充
    // 下沉这里特殊判断最后一个位置

    if (idx > 0 && parent->children[idx - 1]->num_keys >= t) {
        borrow_from_left(parent, idx);
        return parent->children[idx];
    }
    if (idx < parent->num_keys && parent->children[idx + 1]->num_keys >= t) {
        borrow_from_right(parent, idx);
        return parent->children[idx];
    }

    if (idx == parent->num_keys) {
        merge_node(parent, idx - 1);
        return parent->children[idx - 1];
    }
    else {
        merge_node(parent, idx);
        return parent->children[idx];
    }
}


static void remove_node(btree_node_t* node, int key, int t) {

    if (!node) {
        printf("node is null in remove\n");
        exit(-2);
    }

    btree_node_t* current = node;
    int c_idx = -1;
    while (true) {
        c_idx = find_key_idx(current, key);
        btree_node_t* left_child = NULL;
        btree_node_t* right_child = NULL;

        // 找到还是没找到
        // 是否是叶子节点

        if (c_idx < current->num_keys) {
            left_child = current->children[c_idx];
            right_child = current->children[c_idx + 1];
            if (current->keys[c_idx] == key) {
                if (current->is_leaf) {
                    // 直接删除
                    for (int i = c_idx + 1; i < current->num_keys; i++) {
                        current->keys[i - 1] = current->keys[i];
                    }
                    current->num_keys--;
                    return;
                }
                else {
                    // 利用前后驱替换,不能前后驱则用合并下沉
                    if (left_child->num_keys >= t) {
                        // 可以使用前驱替换，前驱有足够的键个数
                        int p_val = get_precursor(current, c_idx);
                        current->keys[c_idx] = p_val;
                        current = left_child;
                        key = p_val;
                        continue;
                    }
                    else if (right_child->num_keys >= t) {
                        int s_val = get_successor(current, c_idx);
                        current->keys[c_idx] = s_val;
                        current = right_child;
                        key = s_val;
                        continue;
                    }
                    else {
                        //前后驱键个数不足,不能直接覆盖，则把当前节点下沉
                        merge_node(current, c_idx);
                        current = current->children[c_idx];
                        continue;
                    }
                }
            }
        }
        // c_idx == num_keys或者c_idx < num_keys && keys[c_idx] < key

        // 这里没有找到键无论是最后一个子区间节点，还是内部子区间节点
        // 都执行相同的逻辑去到该子区间节点
        // 这里不需要判断如果是叶子节点那么child不存在的情况
        // 因为删除的前提一定是该键存在
        btree_node_t* next = current->children[c_idx];
        if (next->num_keys < t) {
            next = fill_node(current, c_idx, t);
        }
        current = next;
    }
}


bool btree_remove(btree_t* tree, int key) {

    if (!tree || !tree->root) {
        printf("tree is null at btree_remove\n");
        exit(-2);
    }
    // not exist return directly true
    if (!btree_contains(tree, key)) {
        return true;
    }
    // 允许根节点键个数大于0就行，所以这里不需要判断根节点是否需要填充
    btree_node_t* root = tree->root;
    remove_node(root, key, tree->t);
    // 根节点键个数为0，降低树高度
    if (root->num_keys == 0 && !root->is_leaf) {
        // 这里如果是最后一个键,没有child，tree->root将会指向NULL
        // 根节点的最后一个键下沉，说明有children,那么root指向child
        tree->root = root->children[0];
        destroy_node(root);
    }
    return true;
}

static void inorder_traversal(btree_node_t* node) {

    if (node->is_leaf) {
        for (int i = 0; i < node->num_keys; i++) {
            printf("%d ", node->keys[i]);
        }
        return;
    }

    for (int i = 0; i <= node->num_keys; i++) {
        inorder_traversal(node->children[i]);
        if (i != node->num_keys) {
            printf("%d ", node->keys[i]);
        }
    }
}


void btree_inorder_traversal(btree_t* tree) {

    if (!tree || !tree->root) {
        printf("tree is null in btree_inorder_traversal\n");
        return;
    }

    inorder_traversal(tree->root);
}








