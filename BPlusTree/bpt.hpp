#ifndef BPT_H
#define BPT_H

#include <stddef.h>
#include <stdio.h>
#include <assert.h>
#include <iostream>
#include <cerrno>
#include <shared_mutex>
#include <string>
#include <utility>
#include <string_view>
extern "C" {
    #include <stdlib.h>
    #include <sys/file.h>    //  flock() 函数声明
    #include <fcntl.h>       //  O_RDWR, O_CREAT 等文件打开标志
    #include <unistd.h>      //  open(), close() 等文件操作函数 fsync()
}


// #ifndef UNIT_TEST
#include "predefined.hpp"
// #else
// #include "util/unit_test_predefined.h"
// #endif

namespace bpt {

/* offsets */
#define OFFSET_META 0
#define OFFSET_BLOCK OFFSET_META + sizeof(meta_t)
// 这里在结构体对齐的情况下也正确，增加的是sizeof(leaf_node_t)的大小
// 不会读少，只可能读多
#define SIZE_NO_CHILDREN sizeof(leaf_node_t) - BP_ORDER * sizeof(record_t)

/* meta information of B+ tree */
typedef struct {
    // 先用最简单的互斥锁来实现进程间安全
    
    size_t order; /* `order` of B+ tree */
    size_t value_size; /* size of value */
    size_t key_size;   /* size of key */
    size_t internal_node_num; /* how many internal nodes */
    size_t leaf_node_num;     /* how many leafs */
    size_t height;            /* height of tree (exclude leafs) */
    off_t slot;        /* where to store new block */
    off_t root_offset; /* where is the root of internal nodes */
    off_t leaf_offset; /* where is the first leaf */
} meta_t;

/* internal nodes' index segment */
struct index_t {
    key_t key;
    off_t child; /* child's offset */
};

/***
 * internal node block
 ***/
struct internal_node_t {
    typedef index_t * child_t;

    off_t parent; /* parent node offset */
    off_t next;
    off_t prev;
    size_t n; /* how many children */
    index_t children[BP_ORDER];
};

/* the final record of value */
struct record_t {
    key_t key;
    value_t value;
};

/* leaf node block */
struct leaf_node_t {
    typedef record_t *child_t;

    off_t parent; /* parent node offset */
    off_t next;
    off_t prev;
    size_t n;
    record_t children[BP_ORDER];
};

/* the encapulated B+ tree */
class bplus_tree {
    public:
    class leaf_iterator;
    using iterator = leaf_iterator;

    bplus_tree::iterator ite_begin() {
        return iterator(path, meta.leaf_offset);
    }
    
    bplus_tree::iterator ite_end() {
        return iterator("", 0, true);
    }
    
    class leaf_iterator {


        FILE *fp_;
        leaf_node_t leaf_node_;
        int n_;
        bool is_end;

    public:
        
        bool operator!=(leaf_iterator const& ite) {
            return this->is_end != ite.is_end;
        }
    
        bool operator==(leaf_iterator const& ite) {
            return this->is_end == ite.is_end;
        }
        
        explicit leaf_iterator(const char* path, off_t current, bool sign = false):
            is_end(sign), fp_(nullptr)
        {
            if (strcmp(path, "") == 0) {
                return;
            }
            fp_ = fopen(path, "rb");
            // fseek(fp_, )
            fseek(fp_, current, SEEK_SET);
            fread(&leaf_node_, sizeof(leaf_node_t), 1, fp_);
            if (leaf_node_.n == 0) {
                is_end = true;
            }
            n_ = 0;
        }
        ~leaf_iterator() {
            if (fp_ != nullptr) {
                fclose(fp_);
            }
        }
        // 这里是返回std::pair
        std::pair<std::string_view, int> operator*() {
            if (n_ < leaf_node_.n) {
                return std::pair<std::string_view, int>(leaf_node_.children[n_].key.k, leaf_node_.children[n_].value);
            }
            return {"", -1};
        }
        
        // 前置++
        leaf_iterator& operator++() {
            if (is_end == true) {
                return *this;
            }
            if (n_ < leaf_node_.n - 1) {
                n_++;
                return *this;
            } 

            // n == leaf_node_.n - 1
            if (leaf_node_.next == 0) {
                is_end = true;
                return *this;
            }
            // 去到下一个块
            fseek(fp_, leaf_node_.next, SEEK_SET);
            fread(&leaf_node_, sizeof(leaf_node_t), 1, fp_);
            n_ = 0;
            return *this;
        }

        // 后置++
        // value_iterator operator++(int)

        
        

    
    };
    bplus_tree(const char *path, bool force_empty = false);

    /* abstract operations */
    int search(const key_t& key, value_t *value) ;//const;
    int search_range(key_t *left, const key_t &right,
                     value_t *values, size_t max, bool *next = NULL) const;
    int remove(const key_t& key);
    int insert(const key_t& key, value_t value);
    int update(const key_t& key, value_t value);
    meta_t get_meta() const {
        return meta;
    };

// #ifndef UNIT_TEST
//     public:
// #else
// public:
// #endif

    std::shared_mutex _mtx;

    char path[512];
    meta_t meta;

    /* init empty tree */
    void init_from_empty();

    /* find index */
    off_t search_index(const key_t &key) const;

    /* find leaf */
    off_t search_leaf(off_t index, const key_t &key) const;
    off_t search_leaf(const key_t &key) const
    {
        return search_leaf(search_index(key), key);
    }

    /* remove internal node */
    void remove_from_index(off_t offset, internal_node_t &node,
                           const key_t &key, bool remove_flag = false);

    /* borrow one key from other internal node */
    bool borrow_key(bool from_right, internal_node_t &borrower,
                    off_t offset);

    /* borrow one record from other leaf */
    bool borrow_key(bool from_right, leaf_node_t &borrower);

    /* change one's parent key to another key */
    void change_parent_child(off_t parent, const key_t &o, const key_t &n);

    /* merge right leaf to left leaf */
    void merge_leafs(leaf_node_t *left, leaf_node_t *right);

    void merge_keys(index_t *where, internal_node_t &left,
                    internal_node_t &right, bool change_where_key = false);

    /* insert into leaf without split */
    void insert_record_no_split(leaf_node_t *leaf,
                                const key_t &key, const value_t &value);

    /* add key to the internal node */
    void insert_key_to_index(off_t offset, const key_t &key,
                             off_t value, off_t after);
    void insert_key_to_index_no_split(internal_node_t &node, const key_t &key,
                                      off_t value);

    /* change children's parent */
    void reset_index_children_parent(index_t *begin, index_t *end,
                                     off_t parent);

    template<class T>
    void node_create(off_t offset, T *node, T *next);

    template<class T>
    void node_remove(T *prev, T *node);

    /* multi-level file open/close */
    mutable FILE *fp;
    // fp的打开文件的引用计数
    mutable int fp_level;
    void open_file(const char *mode = "rb+") const
    {
        std::cout << "open_file begining" << ",fp_level:" << this->fp_level << std::endl;
        // `rb+` will make sure we can write everywhere without truncating file
        
        if (fp_level == 0) {
            printf("open_file -> fp_level,mode:%s\n", mode);
            fp = fopen(path, mode);
            // if (fp == NULL) {
            //     printf("open_file->fp_level == 0->fp == NULL");
            //     fp = fopen(path, "wb+");
            //     init_from_empty()
            //     if (fp == NULL) {
            //         perror("fopen失败,用wb+\n");
            //     }
            // }
        }
        printf("open_file ->++fp_level\n");
        

        ++fp_level;
    }

    void close_file() const
    {
        if (fp_level == 1) {
            if (fp != NULL) {
                fclose(fp);
            }
        }
        if (fp_level == 0) {
            std::cout << "fp_level is zero, call close is failed" << std::endl;
            exit(-10);
        }
        printf("close_file ->--fp_level\n");

        --fp_level;
    }

    /* alloc from disk */
    off_t alloc(size_t size)
    {
        // 拿到当前的偏移量，然后向后偏移指定size,偏移的部分就是需要使用的数据,最后返回该数据的开头，即拿到的slot
        off_t slot = meta.slot;
        meta.slot += size;
        return slot;
    }

    off_t alloc(leaf_node_t *leaf)
    {
        leaf->n = 0;
        meta.leaf_node_num++;
        return alloc(sizeof(leaf_node_t));
    }
//内部节点的一个alloc,这里的n应该是
    off_t alloc(internal_node_t *node)
    {
        // 每次创建n初始为1，也就是children用空字符""的key结尾
        node->n = 1;
        meta.internal_node_num++;
        return alloc(sizeof(internal_node_t));
    }

    void unalloc(leaf_node_t *leaf, off_t offset)
    {
        --meta.leaf_node_num;
    }

    void unalloc(internal_node_t *node, off_t offset)
    {
        --meta.internal_node_num;
    }

    /* read block from disk */
    // 成功返回0，失败返回-1
    int map(void *block, off_t offset, size_t size) const
    {
        std::cout << "int map(void *block, off_t offset, size_t size) const 1" << std::endl;
        open_file();
        if (fp == nullptr) {
            printf("open_file()->fp == nullptr->return -1\n");
            close_file();
            return -1;
        }
        std::cout << "int map(void *block, off_t offset, size_t size) const 2" << std::endl;
        fseek(fp, offset, SEEK_SET);
        std::cout << "int map(void *block, off_t offset, size_t size) const 3" << std::endl;
        size_t rd = fread(block, size, 1, fp);
        std::cout << "int map(void *block, off_t offset, size_t size) const 4" << std::endl;
        close_file();
        std::cout << "int map(void *block, off_t offset, size_t size) const 5" << std::endl;
        return rd - 1;
    }

    template<class T>
    int map(T *block, off_t offset) const
    {
        std::cout << "map(T *block, off_t offset) 1" << std::endl;
        return map(block, offset, sizeof(T));
    }

    /* write block to disk */
    int unmap(void *block, off_t offset, size_t size) const
    {
        std::cout << " unmap(void *block, off_t offset, size_t size) 1" << std::endl;
        open_file();
        std::cout << " unmap(void *block, off_t offset, size_t size) 2" << std::endl;
        
        fseek(fp, offset, SEEK_SET);
        std::cout << " unmap(void *block, off_t offset, size_t size) 3" << std::endl;
        size_t wd = fwrite(block, size, 1, fp);
        std::cout << " unmap(void *block, off_t offset, size_t size) 4" << std::endl;
        close_file();
        std::cout << " unmap(void *block, off_t offset, size_t size) 5" << std::endl;
        return wd - 1;
    }

    template<class T>
    int unmap(T *block, off_t offset) const
    {

        // 把指针所指向的对象的二进制数据写入到文件中，这里大小是sizeof(T)即该对象的大小，从指针所指向的位置开始写
        return unmap(block, offset, sizeof(T));
    }
};

    class TreeStructureDisplay {
    public:
        TreeStructureDisplay(const bplus_tree &tree) : tree(tree) {}

        void Display() {
            displayNode(tree.get_meta().root_offset, 0);
        }

    private:
        const bplus_tree &tree;

        void displayNode(off_t offset, int level) {
            if (offset == 0) {
                return;
            }

            if (level < tree.get_meta().height) {
                // 内部节点
                internal_node_t node;
                tree.map(&node, offset);

                std::cout << indent(level) << "Internal Node at offset " << offset << ": contains " << node.n
                          << " keys\n";
                for (size_t i = 0; i < node.n; ++i) {
                    std::cout << indent(level + 1) << "Key: " << node.children[i].key.k << ", Child: "
                              << node.children[i].child << "\n";
                }

                // 递归遍历子节点
                for (size_t i = 0; i < node.n; ++i) {
                    displayNode(node.children[i].child, level + 1);
                }
            } else {
                // 叶节点
                leaf_node_t leaf;
                tree.map(&leaf, offset);

                std::cout << indent(level) << "Leaf Node at offset " << offset << ": contains " << leaf.n
                          << " records\n";
                for (size_t i = 0; i < leaf.n; ++i) {
                    std::cout << indent(level + 1) << "Record: Key: " << leaf.children[i].key.k << ", Value: "
                              << leaf.children[i].value << "\n";
                }
            }
        }
        //indent
        std::string indent(int level) {
            std::string res;
            for (int i = 0; i < level; ++i) {
                res += "  ";
            }
            return res;
        }
    };
}






#endif /* end of BPT_H */
