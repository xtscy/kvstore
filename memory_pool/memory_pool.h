#ifndef _GNU_SOURCE
#define _GNU_SOURCE
#endif

#ifndef _MEMORY_POOL_
#define _MEMORY_POOL_
#include <errno.h>
#include <string.h>
#include <stddef.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <pthread.h>
#include <stdatomic.h>
#include <unistd.h>
#include <sys/mman.h>

// 这里每个数据块的大小，依据数据类型的不同而不同
// 目前的数据类型有整形，浮点型，字符串
// 可能以后会有其他的数据结构来作为值
// 目前先考虑整形和字符串
// 如果是整形，就用SMALL来对应整形数据.
// 这里也可以不同内存池就对应不同数据类型
// 这里如果使用分级，那么key也分级，就会组合爆炸
// 所以这里按照kv总大小来分级，有效减少组合个数
// 这里分成48，48 -> Integer，浮点
// 字符串使用 1024

#define SMALL_SIZE 48
#define MEDIUM_SIZE 512

#define PAGE_SIZE (4 * 1024)
#define ALIGNMENT 8//* 这里对齐应该是2的倍数
#define ALIGN(size) ((size + (ALIGNMENT - 1)) & (~(ALIGNMENT - 1)))

// typedef struct mem_block mem_block_t;
// typedef struct fixed_size_pool fixed_size_pool_t;





//todo Level 1: 固定大小池（8B, 32B, 64B, 128B, 256B）

//todo Level 2: 动态池（256B-4KB）

//todo Level 3: 大对象直接使用malloc
// 内存块结构
typedef struct mem_block {
    //* 指向下一个内存块
    struct mem_block* next;
    //* 当前块的大小
    size_t data_size;
    //* 使用柔性数组
    unsigned char data[];
} mem_block_t;

// 内存页结构（用于动态池）
typedef struct memory_page {
    //* 指向下一页
    struct memory_page* next;
    //* 当前页的大小
    size_t page_size;
    //* 当前页剩余字节数
    size_t free_bytes;
    //* 指向当前页下一个可用空间
    //* 这个指针是指向data数组的
    void* free_ptr;
    unsigned char data[];
} memory_page_t;

//todo 上面为内存池的底层具象结构
//todo 下面为内存池设计
// 固定大小内存池（Level 1）
typedef struct fixed_size_pool {
    //* 空闲的每一个块
    //* 使用原子计数,保证每次都拿到最新的,不会有例如内存覆盖的问题
    _Atomic(mem_block_t*) free_list;
    //* 开辟内存池的基地址
    void* memory_base;
    //* 固定大小内存池,所以表明每个块大小
    size_t block_size;
    //* 实际的总共块个数
    size_t block_count;
    //* 当前固定内存池,剩余空闲块个数.原子计数保证线程安全
    // 实际映射大小,以页为单位，因为使用了mmap
    size_t actual_mapped_size;
    //* 实际的块的剩余个数
    _Atomic size_t free_blocks;
    //* 锁只在初始化固定大小内存池时使用
    // pthread_mutex_t mutex; // 用于初始化保护
} fixed_size_pool_t;

// 线程本地缓存（Level 1的优化）
typedef struct thread_cache {
    //* 线程本地缓存的空闲内存块链表
    mem_block_t* free_list;
    //* 线程缓存最多能持有的空闲块个数
    size_t max_blocks;
    //* 当前线程缓存中实际持有的空闲块个数
    size_t current_blocks;
    //* 指向全局内存池.当缓存空,向全局申请,当缓存满要释放,就释放给全局pool
    struct fixed_size_pool* global_pool;
} thread_cache_t;

// 动态内存池（Level 2）
typedef struct dynamic_pool {
    //* 指向开辟的内存页链表的头指针 
    memory_page_t* pages;
    //* 用于限制只能处理大小范围内的内存申请请求
    //* 处理的最小快大小
    size_t min_block_size;
    //* 处理的最大块大小
    size_t max_block_size;
    //* 用于线程安全,这里先用互斥锁
    pthread_mutex_t mutex;
} dynamic_pool_t;

// 多级内存池管理器
typedef struct hierarchical_memory_pool {
    // Level 1: 固定大小池（小对象）
    fixed_size_pool_t* small_pools[4];  // 32B, 64B, 128B, 256B
    
    // Level 2: 动态池（中等对象）
    dynamic_pool_t medium_pool;         // 256B - 4KB
    
    // Level 3: 大对象直接分配（>4KB）
    // 直接使用malloc/free
    
    // 线程本地缓存
    pthread_key_t thread_cache_key;
    size_t thread_cache_size;
    
    // 统计信息
    _Atomic size_t total_allocations;
    _Atomic size_t small_allocations;
    _Atomic size_t medium_allocations;
    _Atomic size_t large_allocations;
} hierarchical_pool_t;


extern fixed_size_pool_t* fixed_pool_create(size_t block_size, size_t block_count);
extern void* fixed_pool_alloc(fixed_size_pool_t*);
extern void fixed_pool_free(fixed_size_pool_t*, void*);
extern void fixed_pool_destroy(fixed_size_pool_t*);
extern void fixed_pool_stats(fixed_size_pool_t*);
extern fixed_size_pool_t *global_fixed_pool;


#endif