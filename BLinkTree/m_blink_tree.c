#include "m_blink_tree.h"

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

    pthread_rwlock_init(&node->rw_lock,NULL);
    atomic_store_explicit(&node->meta_valid, false, memory_order_relaxed);
    // atomic_store_explicit(&node->being_split, false, memory_order_relaxed);
    atomic_store_explicit(&node->high_key, __INT32_MAX__, memory_order_relaxed);
    atomic_store_explicit(&node->right_sibling, NULL, memory_order_release);


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
    
    pthread_rwlock_init(&btree->rw_lock, NULL);
    btree_node_t *node = create_node(true, t);
    atomic_store_explicit(&btree->root, node, memory_order_release);
    
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
//* 节点锁由外部进行锁定，这里直接插入就行
// 这里区间节点的right_sibling和high_key都不变所以不用更改
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
//! 这里parent应该在外部锁定，否则如果多个线程触发多个分裂操作，首先就是会破坏数据，导致运行错误
//! 插入操作用写锁

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
    //* 元信息的设置和修改 
    //* 这里是分裂当前left_child，那么left_child和新创建的right_child
    //* 都需要设置元信息吗，当前right_child是新创建的还没有链入树中
    
    //这里中间键上提，parent节点的high_key值由parent的父节点决定,right_sibling由parent的右兄弟决定
    // 这里并不会改变parent节点的位置即右兄弟不变父节点的键值不变
    // 那么high_key不变，right_sibling也不变
    //? 那么需要更改的就是left_child，因为创建了新的right_child即left_child的右兄弟
    //? 那么就需要设置right_child的元信息为left_child，然后再用right_child更新left_child的元信息
    //! 这里当某个操作判断being_split为真，到达新创建的节点时，为了数据一定存在，避免设置了状态还未搬动数据
    //! 设置新节点的分裂标志，这样当到达新节点时依然可以使用元信息继续查询
    //! 那么在标志设置前对新创建的节点加写锁，这样由于搜索操作最后的遍历需要加读锁遍历，就会被阻塞
    //! 从而保证数据的一致性，这里如果接受搜索失败，那么这里就可以不用给新节点加写锁，这样的话读操作
    //! 就可以直接读取，但是可能数据还没有搬运，导致没有搜索到，如果目标在当前节点或者当前节点的子节点的话
    //! 即在分裂标志设置前对新节点加写锁即可，但是就会降低共同的性能，因为当前线程如果直接失败当前任务即完成
    //! 会更少的占用cpu资源
    
    //? 这里有两个标志需要设置，那么由于left_child依赖于right_child
    //? 则先设置right_child再设置left_child
    //? 这里right_child和left_child都需要加写锁，为了数据可见的强一致性
    // pthread_rwlock_wrlock
    // 这里parent已经在外部写锁定，所以不会有原子变量的竞争问题
    // 每一时刻只有一个insert
    
    // 原子更新 ------
    // 
    // atomic_store_explicit(&right_child->being_split, true, memory_order_relaxed);
    // atomic_store_explicit(&left_child->being_split, true, memory_order_relaxed);
    atomic_store_explicit(&left_child->meta_valid, false, memory_order_relaxed);
    int temp_key = atomic_load_explicit(&left_child->high_key, memory_order_relaxed);
    atomic_store_explicit(&right_child->high_key, temp_key, memory_order_relaxed);
    btree_node_t *temp_node = = atomic_load_explicit(&left_child->right_sibling, memory_order_relaxed);
    atomic_store_explicit(&right_child->right_sibling, temp_node, memory_order_relaxed); 
    //? 这里使用了left_child。这里有可能有删除操作涉及到left_child
    
    atomic_store_explicit(&left_child->high_key, temp_key, memory_order_relaxed);
    atomic_store_explicit(&left_child->right_sibling, right_child, memory_order_relaxed);
    atomic_store_explicit(&left_child->meta_valid, true, memory_order_release);

//这里肯定需要锁right，但是left只有一个num_keys的修改，这里是否需要给left_child加锁呢
// 这里外部parent锁定，并且当前left_child要分裂，搜索操作不会更改数据
// 这里删除操作是否会影响呢，这里删除操作也是锁parent所以也不会影响
// 那么这里其实可以不锁left_child, 这样搜索目标在当前分裂节点可以直接搜索，并且数据没有被覆盖
// 因为其他写操作肯定会被阻塞,这里num_keys变量用原子最好,但是下面有release所以保证了可见性
    pthread_rwlock_wrlock(&right_child->rw_lock);
    // 最后更改左子节点的个数
    // 这里如果还没有设置分裂标志，那么search是否使用元信息
    
    
    // ------ 原子更新
    
//todo lock
// 下面复制left_child的数据是否需要这个写锁
// 因为Left_child只需要改变键的个数
// 这里parent外部肯定写锁，那么其他操作必然阻塞
pthread_rwlock_wrlock(&left_child->rw_lock);

//? 这里的keys的更改肯定是需要在锁内的
    left_child->num_keys = t - 1;
    temp_key = left_child->keys[t - 1];
    
//* 移动数据的操作 -------------
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


    //todo 这里只解锁left_child, 这里由于分裂，next未定，先锁next再解锁当前parent
    //todo 即保证锁到下一个节点再解锁上一个节点，加锁顺序性保证了当前操作尽可能的推进，因为这里一旦写，那么其他的写会被阻塞，读不会
    //todo 也就保证了当前插入数据在（涉及当前节点）其他写操作之前
    pthread_rwlock_unlock(&left_child->rw_lock);    
//* -------------移动数据的操作
    //
}



// 这里的插入逻辑
// 先锁住tree的root根指针以便于拿到最新值
// 然后再锁根节点，至于是读锁还是写锁，由当前的方法而定
// 这里先锁住下个节点，再释放上一个锁
// 由于顺序性，使得当前操作不会有饥饿的情况
// 因为锁住了上一个节点，如果当前是写操作那么其他操作想要锁上一个节点的时候
// 就会锁阻塞
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


    //* 锁根指针,写锁因为可能会更新根指针
    pthread_rwlock_wrlock(&tree->rw_lock);
    
    btree_node_t* current = tree->root;
    
    int t = tree->t;
    int max = 2 * t - 1;

    // 这里判断根节点是否满，如果满把中间键上提到新的root节点
    if (current->num_keys == 2 * t - 1) {
        btree_node_t* new_root = create_node(false, t);// new root 不需要锁,还没连接进树结构
        // 让new_root指向当前root
        new_root->children[0] = current;
        pthread_rwlock_wrlock(&current->rw_lock);//* 分裂锁根节点
        // 这里可以不用锁新节点，所以多传一个参数用来控制是否需要锁parent节点
        split_child(new_root, 0, current, t, 0);
        pthread_rwlock_unlock(&current->rw_lock);
        tree->root = new_root;
        current = new_root;
    }


//* 解锁根指针锁
    pthread_rwlock_unlock(&tree->rw_lock);
    
//* 这里同样先去使用optimstic的contain函数
//* 因为写操作的开销在多线程下的频繁加锁是开销极大的

    
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

        //多线程下的安全判断
        if (current->keys[index] == key) return false;

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

    
// 这种contains可以在当前数据存在(可能正在被删除)的情况下也返回false
// 这样至少能够在数据存在的时候一定返回false，减少insert开销

//这里可能值目前不存在但是其他操作正在插入该数据，但是contains判断不存在
//所以还需要在插入操作中多一个判断去判断值是否相等，相等则返回false

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
// 这里需要处理'往下走时，可能当前下面的节点正在执行写操作(增加或者删除)
// 这里可能当前删除操作在对同一个节点的删除操作前
// 所以在删除中最开始先判断是否有节点存在，然后在删除内部还需要保证节点不存在的操作

// 这里的拿前驱,可能增加的数据是最新的前驱,如果拿到的是旧数据
// 这里可以优化，直接返回值和指向前驱所在的叶子节点
// 然后返回，在删除操作中直接删除当前节点，这里后续优化
// 这里的向下过程的加锁是完全有必要的，为了确保拿到最新的写操作之后的前驱的最新值
static int get_precursor(btree_node_t* node, int idx) {

    if (!node) {
        printf("node is null at get_successor\n");
        exit(-2);
    }//? node不是叶子外部保证(这里外部如果是叶子节点就直接删除了)
    // //error（这里是用remove前的contain但是多线程下contain是乐观逻辑所以不能保证）
    //? 这里依次加解锁，获得最新前驱
    // node是在外部进行lock的,进来就是被lock的状态
    // 这里有可能前面的删除操作正好使得叶子节点为空，那么就需要去具体判断remove的逻辑
    // 这里remove在调用remove_node删除之后,才会去
    // 并且删除操作不会锁定之前的节点，
    btree_node_t* current = node->children[idx];
    btree_node_t* next = NULL;
    pthread_rwlock_wrlock(&current->rw_lock);
    // pthread_rwlock_unlock(&node->rw_lock);
    while (!current->is_leaf) {
        next = current->children[current->num_keys];
        pthread_rwlock_wrlock(&next->rw_lock);
        pthread_rwlock_unlock(&current);
        current = current->children[current->num_keys];
    }
    int ret = current->keys[current->num_keys - 1];
    return ret;
}

// 拿到后继的最小值节点
// 后继的逻辑和上面的前驱一致
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
// 这里传入前，parent在外部已经锁定
// 其中使用的merge_node也是涉及parent
// 对于左右兄弟，这里在内部涉及时再去锁定
static btree_node_t* fill_node(btree_node_t* parent, int idx, int t) {

    btree_node_t* target = parent->children[idx];
    // 这里idx在num_keys和小于num_keys
    // 这里先借左兄弟和右兄弟，如果不能借，则用下沉来填充
    // 下沉这里特殊判断最后一个位置

    if (idx > 0 && parent->children[idx - 1]->num_keys >= t) {
        //lock
        borrow_from_left(parent, idx);
        return parent->children[idx];
    }
    if (idx < parent->num_keys && parent->children[idx + 1]->num_keys >= t) {
        //lock
        borrow_from_right(parent, idx);
        return parent->children[idx];
    }

    if (idx == parent->num_keys) {
        pthread_rwlock_wrlock(&parent->children[idx - 1]->rw_lock);
        //merge_node内部不加锁，为了统一使用同一个merge_node
        merge_node(parent, idx - 1);
        return parent->children[idx - 1];
    }
    else {
        merge_node(parent, idx);
        return parent->children[idx];
    }
}



//? 删除锁parent(cur),这里merge调用前肯定需要锁定当前cur
static bool remove_node(btree_node_t* node, int key, int t) {

    if (!node) {
        printf("node is null in remove\n");
        exit(-2);
    }

    btree_node_t* current = node;
    int c_idx = -1;
    //*? 可以先用读锁，再升级写锁
    //*? 也可以直接用写锁,这里先用写锁
    pthread_rwlock_wrlock(&current->rw_lock);
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
                    pthread_rwlock_unlock(&current->rw_lock);
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
        // ?下去之前先锁定
        btree_node_t* next = current->children[c_idx];
        // 读取用读锁,这里直接用写锁，这里如果内部升级，数据状态可能改变，读锁和写锁之间有时间窗口，并且可能写锁竞争失败，可能有其他线程也需要该写锁
        // 同时还有内部就算升级成功，也可能有其他读锁存在，这里直接加写锁，减少当前涉及节点的读锁锁定
        pthread_rwlock_wrlock(&next->rw_lock);
        if (next->num_keys < t) {
            next = fill_node(current, c_idx, t);
        }
        current = next;
    }
}


bool btree_remove(btree_t* tree, int key) {

    if (!tree || !tree->root || tree->root->num_keys == 0) {
        printf("tree is null at btree_remove\n");
        exit(-2);
    }
    // not exist return directly true
    if (!btree_contains(tree, key)) {
        return true;
    }
    // 允许根节点键个数大于0就行，所以这里不需要判断根节点是否需要填充
    btree_node_t* root = tree->root;
    //
    bool ret = remove_node(root, key, tree->t);
    // 根节点键个数为0，降低树高度
    // 这里叶子节点
    if (root->num_keys == 0 && !root->is_leaf) {
        // 这里如果是最后一6个键,没有child，tree->root将会指向NULL
        // 根节点的最后一个键下沉，说明有children,那么root指向child
        tree->root = root->children[0];
        destroy_node(root);
    }
    return true;
}

int test_idx = 0;

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

static void inorder_traversal_test_array(btree_node_t* node, int *test_array) {

    if (node->is_leaf) {
        for (int i = 0; i < node->num_keys; i++) {
            if (test_array[test_idx++] == node->keys[i]) {
                printf("%d ", node->keys[i]);
            } else {
                printf("%d 所在位置错误\n", node->keys[i]);
                exit(-10);
            }
        }
        return;
    }

    for (int i = 0; i <= node->num_keys; i++) {
        inorder_traversal_test_array(node->children[i], test_array);
        if (i != node->num_keys) {
            // printf("%d ", node->keys[i]);
            if (test_array[test_idx++] == node->keys[i]) {
                printf("%d ", node->keys[i]);
            } else {
                printf("%d 所在位置错误\n", node->keys[i]);
                exit(-10);
            }
        }
    }
}


void btree_inorder_traversal_test_array(btree_t* tree, int *test_array) {

    if (!tree || !tree->root) {
        printf("tree is null in btree_inorder_traversal\n");
        return;
    }

    inorder_traversal_test_array(tree->root, test_array);
}








