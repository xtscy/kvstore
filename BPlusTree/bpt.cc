#include "bpt.hpp"

#include <stdlib.h>

#include <list>
#include <algorithm>
using std::swap;
using std::binary_search;
using std::lower_bound;
using std::upper_bound;

namespace bpt {

/* custom compare operator for STL algorithms */
OPERATOR_KEYCMP(index_t)
OPERATOR_KEYCMP(record_t)

/* helper iterating function */
template<class T>
// 迭代器返回begin和last
inline typename T::child_t begin(T &node) {
    return node.children; 
}

template<class T>
inline typename T::child_t end(T &node) {
    return node.children + node.n;
}

/* helper searching function */
//* 针对于内部节点的find函数
inline index_t *find(internal_node_t &node, const key_t &key, bool flag = false) {
    if (key) {// 索引的作为结束的最后一个key是一个空字符"", 所以这里find的时候区分开来
        // upper_bound获得第一个大于Key的
        // lower_bound获得第一个大于等于key的
        // 这里去node节点查找对应的key
        //? 这里upper_bound是左闭右开真的需要end(node) - 1吗
        //? 先这样，代码理清再来解决这个问题
        index_t* where = upper_bound(begin(node), end(node) - 1, key);
        // 
        if (flag ) where --;
        return where;
    }
    // 如果查找的是空字符串，那么需要返回倒数第二个
    // because the end of the index range is an empty string, so if we search the empty key(when merge internal nodes), we need to return the second last one
    // 返回的是当前node节点的倒数第二个
    if (node.n > 1) {
        return node.children + node.n - 2;
    }
    return begin(node);
}
//* 针对于叶子节点的find函数，直接返回区间中第一个>=key的位置
inline record_t *find(leaf_node_t &node, const key_t &key) {
    return lower_bound(begin(node), end(node), key);
}

bplus_tree::bplus_tree(const char *p, bool force_empty)
    : fp(NULL), fp_level(0)
{
    std::cout << "bplus_tree 1" << std::endl;
    bzero(path, sizeof(path));
    // 把存在的或打算把文件放到的位置，传过来
    strcpy(path, p);
    std::cout << "bplus_tree 2" << std::string(p) << std::endl;
    std::cout << "bplus_tree 2" << path << std::endl;


    // 如果本身有文件那么就读取原本的树的meta元信息
    // 不强制为空，则去fread读取meta
    if (!force_empty) {
        std::cout << "bplus_tree 3" << std::endl;

        // read tree from file
        // 失败返回-1，读取失败，那么强制为空
        // 如果读取本地文件的元信息成功
        if (bpt::bplus_tree::map(&meta, OFFSET_META) != 0) {
            std::cout << "bplus_tree 4" << std::endl;
            force_empty = true;
        }
        std::cout << "bplus_tree 5" << std::endl;
 
    }
// 用w+,文件内容全部丢失,指针初始位置文件开头
    if (force_empty) {
        std::cout << "bplus_tree 6" << std::endl;

        open_file("wb+"); // truncate file
        // 空b+tree,初始化空树,设置元信息
        // create empty tree if file doesn't exist
        std::cout << "bplus_tree 7" << std::endl;
        
        init_from_empty();
        std::cout << "bplus_tree 8" << std::endl;
        // 设置成功,写入了btree元信息,起始的根节点和起始的叶子节点的信息
        close_file();
    }

}

int bplus_tree::search(const key_t& key, value_t *value) //const
{
    _mtx.lock_shared();
    open_file();
    int fd = fileno(fp);
    flock(fd, LOCK_SH);
    map(&(this->meta), OFFSET_META);

    leaf_node_t leaf;
    map(&leaf, search_leaf(key));

    // finding the record
    record_t *record = find(leaf, key);
    flock(fd, LOCK_UN);
    close_file();
    _mtx.unlock_shared();
    if (record != leaf.children + leaf.n) {
        // always return the lower bound
        *value = record->value;

        return keycmp(record->key, key);
    } else {
        return -1;
    }
    
}

int bplus_tree::search_range(key_t *left, const key_t &right,
                             value_t *values, size_t max, bool *next) const
{
    if (left == NULL || keycmp(*left, right) > 0)
        return -1;

    off_t off_left = search_leaf(*left);
    off_t off_right = search_leaf(right);
    off_t off = off_left;
    size_t i = 0;
    record_t *b, *e;

    leaf_node_t leaf;
    while (off != off_right && off != 0 && i < max) {
        map(&leaf, off);

        // start point
        if (off_left == off) 
            b = find(leaf, *left);
        else
            b = begin(leaf);

        // copy
        e = leaf.children + leaf.n;
        for (; b != e && i < max; ++b, ++i)
            values[i] = b->value;

        off = leaf.next;
    }

    // the last leaf
    if (i < max) {
        map(&leaf, off_right);

        b = find(leaf, *left);
        e = upper_bound(begin(leaf), end(leaf), right);
        for (; b != e && i < max; ++b, ++i)
            values[i] = b->value;
    }

    // mark for next iteration
    if (next != NULL) {
        if (i == max && b != e) {
            *next = true;
            *left = b->key;
        } else {
            *next = false;
        }
    }

    return i;
}

int bplus_tree::remove(const key_t& key)
{
    internal_node_t parent;
    leaf_node_t leaf;

    // find parent node
    off_t parent_off = search_index(key);
    map(&parent, parent_off);

    // find current node
    index_t *where = find(parent, key);
    off_t offset = where->child;
    map(&leaf, offset);

    // verify
    if (!binary_search(begin(leaf), end(leaf), key))
        return -1;

    size_t min_n = meta.leaf_node_num == 1 ? 0 : meta.order / 2;
    assert(leaf.n >= min_n && leaf.n <= meta.order);

    // delete the key
    record_t *to_delete = find(leaf, key);
    std::copy(to_delete + 1, end(leaf), to_delete);
    leaf.n--;

    // merge or borrow
    if (leaf.n < min_n) {
        // first borrow from left
        bool borrowed = false;
        if (leaf.prev != 0)
            borrowed = borrow_key(false, leaf);

        // then borrow from right
        if (!borrowed && leaf.next != 0)
            borrowed = borrow_key(true, leaf);

        // finally we merge
        if (!borrowed) {
            assert(leaf.next != 0 || leaf.prev != 0);

            key_t index_key;

            if (where == end(parent) - 1) {
                // if leaf is last element then merge | prev | leaf |
                assert(leaf.prev != 0);
                leaf_node_t prev;
                map(&prev, leaf.prev);
                index_key = begin(prev)->key;

                merge_leafs(&prev, &leaf);
                node_remove(&prev, &leaf);
                unmap(&prev, leaf.prev);
            } else {
                // else merge | leaf | next |
                assert(leaf.next != 0);
                leaf_node_t next;
                map(&next, leaf.next);
                index_key = begin(leaf)->key;

                merge_leafs(&leaf, &next);
                node_remove(&leaf, &next);
                unmap(&leaf, offset);
            }

            // remove parent's key
            remove_from_index(parent_off, parent, index_key);
        } else {
            unmap(&leaf, offset);
        }
    } else {
        unmap(&leaf, offset);
    }

    return 0;
}

// 插入键key和值value
int bplus_tree::insert(const key_t& key, value_t value)
{
    _mtx.lock();
    // 先直接使用flock锁定文件
    open_file();
    int fd = fileno(fp);
    if (flock(fd, LOCK_EX) == -1) {
        perror("flock LOCK_EX failed\n");
        close_file();
        _mtx.unlock();
        return -1;
    }
    map(&meta, OFFSET_META);

    off_t parent = search_index(key);
    off_t offset = search_leaf(parent, key);
    leaf_node_t leaf;
    map(&leaf, offset);

    // check if we have the same key
    if (binary_search(begin(leaf), end(leaf), key) == true) {
        record_t *record = find(leaf, key);
        if (record->value != value) {
            record->value = value;
            unmap(&leaf, offset);
        }
        fsync(fd);
        flock(fd, LOCK_UN);
        close_file();
        _mtx.unlock();
        abort();
        std::cout << "value same" << std::endl;
        return 1;// 1表示值存在，修改值
        // return 0;
    }

    if (leaf.n == meta.order) {
        // split when full

        // new sibling leaf
        leaf_node_t new_leaf;
        
        // offset为key所在的叶子节点的偏移量
        // 创建right child 
        node_create(offset, &leaf, &new_leaf);

        
        // find even split point
        // 这里使用数量/2获得均分点
        // 偶数个时获得取中间两个的右边
        // 奇数个时取中间
        // 这里用最后一个的下标，奇数不变，偶数取中间的左边
        size_t point = leaf.n / 2;
        // point为指向均分点的下标大小,下面比较key和均分点的key大小
        // 如果key大于均分点那么让point++,则指向了下一个child节点
        //  如果小于等于那么就在当前均分点
        bool place_right = keycmp(key, leaf.children[point].key) > 0;
        if (place_right)
            ++point;

        // split
        //copy是左闭右开，所以这里copy了从point到区间最后一个对象
        std::copy(leaf.children + point, leaf.children + leaf.n,
                  new_leaf.children);

        // 这里给新节点的n值赋值为copy的个数,leaf.n - point = leaf.n - 1 - point + 1
        new_leaf.n = leaf.n - point;
        // 给原本的leaf把孩子数量重新赋值，point下标表示前面的总个数
        // 叶子的孩子是key和value,即叶子节点内部没有指向child的指针，只存储了对应的key和值
        // 传统b树是一个节点内部既有路径索引的key也有指向child区间的指针，还有每个key所对应的值value
        leaf.n = point;

        // which part do we put the key
        // 这里是把要增添的key放置到原本的叶子节点还是新的右边的节点
        // 基于上面的判断，如果当前key是大于原本leaf的均分点的key那么放到右边，否则小于等于均分点key的都放到左边
        if (place_right)
            insert_record_no_split(&new_leaf, key, value);
        else
            insert_record_no_split(&leaf, key, value);

        // save leafs
        unmap(&leaf, offset);
        unmap(&new_leaf, leaf.next);

        // insert new index key
        insert_key_to_index(parent, new_leaf.children[0].key,
                            offset, leaf.next);
    } else {
        insert_record_no_split(&leaf, key, value);
        unmap(&leaf, offset);
    }

    fsync(fd);
    flock(fd, LOCK_UN);
    close_file();
    _mtx.unlock();
    return 0;
}

int bplus_tree::update(const key_t& key, value_t value)
{
    off_t offset = search_leaf(key);
    leaf_node_t leaf;
    map(&leaf, offset);

    record_t *record = find(leaf, key);
    if (record != leaf.children + leaf.n)
        if (keycmp(key, record->key) == 0) {
            record->value = value;
            unmap(&leaf, offset);

            return 0;
        } else {
            return 1;
        }
    else
        return -1;
}

void bplus_tree::remove_from_index(off_t offset, internal_node_t &node,
                                   const key_t &key, bool remove_flag)
{
    size_t min_n = meta.root_offset == offset ? 1 : meta.order / 2;
    assert(node.n >= min_n && node.n <= meta.order);

    // remove key
    key_t index_key = begin(node)->key;
    index_t *to_delete = find(node, key, remove_flag);
    if (to_delete != end(node)) {
        (to_delete + 1)->child = to_delete->child;
        std::copy(to_delete + 1, end(node), to_delete);
    }
    node.n--;

    // remove to only one key
    if (node.n == 1 && meta.root_offset == offset &&
                       meta.internal_node_num != 1)
    {
        unalloc(&node, meta.root_offset);
        meta.height--;
        meta.root_offset = node.children[0].child;
        unmap(&meta, OFFSET_META);
        return;
    }

    // merge or borrow
    if (node.n < min_n) {
        internal_node_t parent;
        map(&parent, node.parent);

        // first borrow from left
        bool borrowed = false;
        if (offset != begin(parent)->child)
            borrowed = borrow_key(false, node, offset);

        // then borrow from right
        if (!borrowed && offset != (end(parent) - 1)->child)
            borrowed = borrow_key(true, node, offset);

        // finally we merge
        if (!borrowed) {
            assert(node.next != 0 || node.prev != 0);
            bool remove_flag = false;

            if (offset == (end(parent) - 1)->child) {
                // if leaf is last element then merge | prev | leaf |
                assert(node.prev != 0);
                internal_node_t prev;
                map(&prev, node.prev);

                // merge
                index_t *where = find(parent, begin(prev)->key);
                reset_index_children_parent(begin(node), end(node), node.prev);
                merge_keys(where, prev, node, true);
                unmap(&prev, node.prev);
            } else {
                // else merge | leaf | next |
                assert(node.next != 0);
                internal_node_t next;
                map(&next, node.next);

                // merge
                index_key = begin(next)->key;
                remove_flag = true;
                index_t *where = find(parent, index_key);
                reset_index_children_parent(begin(next), end(next), offset);
                merge_keys(where, node, next);
                unmap(&node, offset);
            }

            // remove parent's key
            remove_from_index(node.parent, parent, index_key,remove_flag);
        } else {
            unmap(&node, offset);
        }
    } else {
        unmap(&node, offset);
    }
}

bool bplus_tree::borrow_key(bool from_right, internal_node_t &borrower,
                            off_t offset)
{
    typedef typename internal_node_t::child_t child_t;

    off_t lender_off = from_right ? borrower.next : borrower.prev;
    internal_node_t lender;
    map(&lender, lender_off);

    assert(lender.n >= meta.order / 2);
    if (lender.n != meta.order / 2) {
        child_t where_to_lend, where_to_put;

        internal_node_t parent;

        // swap keys, draw on paper to see why
        if (from_right) {
            where_to_lend = begin(lender);
            where_to_put = end(borrower);

            map(&parent, borrower.parent);
            child_t where = find(parent, begin(lender)->key,true);
            where->key = where_to_lend->key;
            unmap(&parent, borrower.parent);
        } else {
            where_to_lend = end(lender) - 1;
            where_to_put = begin(borrower);

            map(&parent, lender.parent);
            child_t where = find(parent, begin(lender)->key);
            // where_to_put->key = where->key;  // We shouldn't change where_to_put->key, because it just records the largest info but we only changes a new one which have been the smallest one
            where->key = (where_to_lend - 1)->key;
            unmap(&parent, lender.parent);
        }

        // store
        std::copy_backward(where_to_put, end(borrower), end(borrower) + 1);
        *where_to_put = *where_to_lend;
        borrower.n++;

        // erase
        reset_index_children_parent(where_to_lend, where_to_lend + 1, offset);
        std::copy(where_to_lend + 1, end(lender), where_to_lend);
        lender.n--;
        unmap(&lender, lender_off);
        return true;
    }

    return false;
}

bool bplus_tree::borrow_key(bool from_right, leaf_node_t &borrower)
{
    off_t lender_off = from_right ? borrower.next : borrower.prev;
    leaf_node_t lender;
    map(&lender, lender_off);

    assert(lender.n >= meta.order / 2);
    if (lender.n != meta.order / 2) {
        typename leaf_node_t::child_t where_to_lend, where_to_put;

        // decide offset and update parent's index key
        if (from_right) {
            where_to_lend = begin(lender);
            where_to_put = end(borrower);
            change_parent_child(borrower.parent, begin(borrower)->key,
                                lender.children[1].key);
        } else {
            where_to_lend = end(lender) - 1;
            where_to_put = begin(borrower);
            change_parent_child(lender.parent, begin(lender)->key,
                                where_to_lend->key);
        }

        // store
        std::copy_backward(where_to_put, end(borrower), end(borrower) + 1);
        *where_to_put = *where_to_lend;
        borrower.n++;

        // erase
        std::copy(where_to_lend + 1, end(lender), where_to_lend);
        lender.n--;
        unmap(&lender, lender_off);
        return true;
    }

    return false;
}

void bplus_tree::change_parent_child(off_t parent, const key_t &o,
                                     const key_t &n)
{
    internal_node_t node;
    map(&node, parent);

    index_t *w = find(node, o);
    assert(w != node.children + node.n); 

    w->key = n;
    unmap(&node, parent);
    if (w == node.children + node.n - 1) {
        change_parent_child(node.parent, o, n);
    }
}

void bplus_tree::merge_leafs(leaf_node_t *left, leaf_node_t *right)
{
    std::copy(begin(*right), end(*right), end(*left));
    left->n += right->n;
}

void bplus_tree::merge_keys(index_t *where,
                            internal_node_t &node, internal_node_t &next, bool change_where_key)
{
    //(end(node) - 1)->key = where->key;
    if (change_where_key) {
        where->key = (end(next) - 1)->key;
    }
    std::copy(begin(next), end(next), end(node));
    node.n += next.n;
    node_remove(&node, &next);
}

// 传要插入的叶子节点leaf,插入的键key和值value
void bplus_tree::insert_record_no_split(leaf_node_t *leaf,
                                        const key_t &key, const value_t &value)
{
    record_t *where = upper_bound(begin(*leaf), end(*leaf), key);
    std::copy_backward(where, end(*leaf), end(*leaf) + 1);

    where->key = key;
    where->value = value;
    leaf->n++;
}

void bplus_tree:: insert_key_to_index(off_t offset, const key_t &key,
                                     off_t old, off_t after)
{

    if (offset == 0) {
        // create new root node
        internal_node_t root;
        root.next = root.prev = root.parent = 0;
        meta.root_offset = alloc(&root);
        meta.height++;

        // insert `old` and `after`
        root.n = 2;
        root.children[0].key = key;
        root.children[0].child = old;
        root.children[1].child = after;

        unmap(&meta, OFFSET_META);
        unmap(&root, meta.root_offset);

        // update children's parent
        reset_index_children_parent(begin(root), end(root),
                                    meta.root_offset);
        return;
    }


// node指向父节点
    internal_node_t node;
    map(&node, offset);
    assert(node.n <= meta.order);

    // 父节点孩子个数=阶，说明当前父节点满了，不能再添加Key
    if (node.n == meta.order) {
        // split when full

        // 用node去构建new_node，这里new_node就是node的右兄弟
        internal_node_t new_node;
        // node_create只是元信息的更新，但是数组值并未更改
        node_create(offset, &node, &new_node);

        
        // find even split point
        // 这里偶数取左边
        size_t point = (node.n - 1) / 2;
        bool place_right = keycmp(key, node.children[point].key) > 0;
        if (place_right)
            ++point;

        // prevent the `key` being the right `middle_key`
        // example: insert 48 into |42|45| 6|  |
        if (place_right && keycmp(key, node.children[point].key) < 0)
            point--;

        key_t middle_key = node.children[point].key;

        // split
        std::copy(begin(node) + point + 1, end(node), begin(new_node));
        new_node.n = node.n - point - 1;
        node.n = point + 1;

        // put the new key
        if (place_right)
            insert_key_to_index_no_split(new_node, key, after);
        else
            insert_key_to_index_no_split(node, key, after);

        unmap(&node, offset);
        unmap(&new_node, node.next);

        // update children's parent
        reset_index_children_parent(begin(new_node), end(new_node), node.next);

        // give the middle key to the parent
        // note: middle key's child is reserved
        insert_key_to_index(node.parent, middle_key, offset, node.next);
    } else {
        insert_key_to_index_no_split(node, key, after);
        unmap(&node, offset);
    }
}

void bplus_tree::insert_key_to_index_no_split(internal_node_t &node,
                                              const key_t &key, off_t value)
{
    index_t *where = upper_bound(begin(node), end(node) - 1, key);

    // move later index forward
    std::copy_backward(where, end(node), end(node) + 1);
    // 当这里要去索引这里的新增key的时候

    // insert this key      
    // 插入的key小于原本where指向的key
    // copy_backward把原本where的键向后移动了一位,这里where+1还是原本的key
    // 这里让大于插入的key的键位置的child指向
    where->key = key;
    where->child = (where + 1)->child;
    (where + 1)->child = value;

    node.n++;
}

void bplus_tree::reset_index_children_parent(index_t *begin, index_t *end,
                                             off_t parent)
{
    // this function can change both internal_node_t and leaf_node_t's parent
    // field, but we should ensure that:
    // 1. sizeof(internal_node_t) <= sizeof(leaf_node_t)
    // 2. parent field is placed in the beginning and have same size
    internal_node_t node;
    while (begin != end) {
        map(&node, begin->child);
        node.parent = parent;
        unmap(&node, begin->child, SIZE_NO_CHILDREN);
        ++begin;
    }
}
 
off_t bplus_tree::search_index(const key_t &key) const
{
    // 拿到根节点的起始偏移量
    off_t org = meta.root_offset;
    // 当前树的高度
    int height = meta.height;
    while (height > 1) {
        internal_node_t node;
        map(&node, org);
// 
        index_t *i = upper_bound(begin(node), end(node) - 1, key);
        org = i->child;
        --height;
    }

    return org;
}

off_t bplus_tree::search_leaf(off_t index, const key_t &key) const
{
    internal_node_t node;
    map(&node, index);
// 在children数组中,key所在的叶子节点被大于key的第一个index_t的child所指向
    index_t *i = upper_bound(begin(node), end(node) - 1, key);
    return i->child;
}


template<class T>
/*
    next为要分裂的right child
    node为当前的分裂节点
    offset为node的偏移量
*/
void bplus_tree::node_create(off_t offset, T *node, T *next)
{
    // new sibling node
    next->parent = node->parent;
    next->next = node->next;
    next->prev = offset;
    node->next = alloc(next);
    // update next node's prev
    if (next->next != 0) {
        T old_next;
        map(&old_next, next->next, SIZE_NO_CHILDREN);
        old_next.prev = node->next;
        // 
        unmap(&old_next, next->next, SIZE_NO_CHILDREN);
        
    }
    // 更新磁盘存储的元信息
    unmap(&meta, OFFSET_META);
}

template<class T>
void bplus_tree::node_remove(T *prev, T *node)
{
    unalloc(node, prev->next);
    prev->next = node->next;
    if (node->next != 0) {
        T next;
        map(&next, node->next, SIZE_NO_CHILDREN);
        next.prev = node->prev;
        unmap(&next, node->next, SIZE_NO_CHILDREN);
    }
    unmap(&meta, OFFSET_META);
}

void bplus_tree::init_from_empty()
{
    // init default meta
    std::cout << "init_from_empty() 1" << std::endl;
    bzero(&meta, sizeof(meta_t));
    meta.order = BP_ORDER;
    meta.value_size = sizeof(value_t);
    meta.key_size = sizeof(key_t);
    meta.height = 1;
    meta.slot = OFFSET_BLOCK;// 先存储树的meta元信息

    // init root node
    // 根节点是内部节点，这里是设计成，叶子节点单独在最后一层
    internal_node_t root;//默认构造n=1,以空字符节点结尾
    root.next = root.prev = root.parent = 0; 
    // alloc 内部节点，这里的root就是一个内部节点
    std::cout << "init_from_empty() 1" << std::endl;
    meta.root_offset = alloc(&root);
    std::cout << "init_from_empty() 2" << std::endl;
    // 这里对于根和叶子节点，这里初始化的时候都有一个起始位置
    // 因为叶子在最后一层也是链式的，所以这里init中去创建根节点和叶子节点两个的起始偏移量
    // init empty leaf
    leaf_node_t leaf;//默认构造n=0,不以""空字符节点结尾
    leaf.next = leaf.prev = 0;
    // 让叶子指向根节点的起始偏移量位置
    leaf.parent = meta.root_offset;
    // 这里设置根节点的第一个孩子指向起始叶子的偏移量
    std::cout << "init_from_empty() 3" << std::endl;
    meta.leaf_offset = root.children[0].child = alloc(&leaf);
    std::cout << "init_from_empty() 4" << std::endl;
     
    // save
    // 这里内部调用fwrite把传入的对象进行写入
    unmap(&meta, OFFSET_META);
    std::cout << "init_from_empty() 5" << std::endl;
    unmap(&root, meta.root_offset);
    std::cout << "init_from_empty() 6" << std::endl;
    unmap(&leaf, root.children[0].child);
    std::cout << "init_from_empty() 7" << std::endl;
}

}
