#include "memory_pool.h"


//* 如果获取失败默认为4096(4KB)
size_t get_sys_page_size() {
    size_t page_size = 0;
    page_size = sysconf(_SC_PAGESIZE);
    if (page_size <= 0) {
        return 4096;
    }   
    return page_size;
}

size_t align_to_page_size(size_t size) {
    size_t page_size = get_sys_page_size();
    //-1是为了处理size刚好是对齐的情况，使得在向上取整的同时，不超过1个单位
    // 由于一定page_size一定是8的倍数。适用于2的幂次方的对齐
    return (size + (page_size - 1)) & ~(page_size - 1);
}

//**固定内存池 */
fixed_size_pool_t* fixed_pool_create(size_t block_data_size, size_t block_count) {
    //* pool 管理结构体使用malloc开辟，真正的数据空间用mmap

    fixed_size_pool_t *pool = malloc(sizeof(fixed_size_pool_t));
    if (pool == NULL) {
        perror("fixed size pool malloc failed\n");
    }
    size_t block_size = block_data_size + sizeof(mem_block_t);
    size_t align_size = ALIGN(block_size);
    
    size_t total_size = align_size * block_count;
    size_t align_page_total_size = align_to_page_size(total_size);
    printf("align_page_total_size: %lu\n", align_page_total_size);
    printf("per_block_size: %lu, total_block_size: %lu\n", align_size, total_size);
    void * memory = mmap(NULL, align_page_total_size, PROT_READ | PROT_WRITE, MAP_PRIVATE | MAP_ANONYMOUS, -1, 0);
    if (!memory) {
        perror("memory failed\n");
        exit(-1);
    }
    pool->actual_mapped_size = align_page_total_size;
    pool->memory_base = memory;
    // pool->block_count = block_count;
    pool->block_size = align_size;
    // atomic_init(&pool->free_blocks, block_count);
    atomic_init(&pool->free_list, NULL);
    // pthread_mutex_init(&pool->mutex, NULL);
    // lock free insert linklist
    char* current = (char*)memory;
    mem_block_t *block = NULL;
    size_t actual_block_count = 0;
    while (current + align_size <= ((char*)pool->memory_base) + align_page_total_size) {
        block = (mem_block_t*) current;
        block->data_size = align_size;
        while (!atomic_compare_exchange_weak(&pool->free_list, &block->next, block)) {
            // CAS失败自动更新next到最新
        }
        current += align_size;
        actual_block_count++;
    }
    // 全部链接到free_list
    pool->block_count = actual_block_count;
    atomic_init(&pool->free_blocks, actual_block_count);

    return pool;

}

// 从固定池分配内存（无锁）
//**返回值为块的数据区域指针 */
void* fixed_pool_alloc(fixed_size_pool_t* pool) {
    mem_block_t* block;
    //* 无锁链表的头部出队列操作
    //* z这里free_list为空，那么block拿到空，返回NULL，表示池已经空
    do {
        block = atomic_load(&pool->free_list);
        if (!block) {
            return NULL; // 池已空
        }
    } while (!atomic_compare_exchange_weak(&pool->free_list, &block, block->next));
    
    atomic_fetch_sub(&pool->free_blocks, 1);
    
    //* 这里是直接返回每个块结构体的存储数据的区域
    return block->data;
}



//? 传递的ptr是block块的数据区域
void fixed_pool_free(fixed_size_pool_t* pool, void* ptr_d) {
    //这里计算出block的起始地址
    // 由于使用了柔性数组，不能使用offsetof
    mem_block_t *block = (mem_block_t*)((char*)ptr_d - sizeof(mem_block_t));
    block->next = atomic_load(&pool->free_list);
    
    while (!atomic_compare_exchange_weak(&pool->free_list, &block->next, block)) {
        // CAS 失败，block->next会更新成free_list的最新值.如果free_list和next指向同一个，那么让list指向block
    }
    // 空闲块+1
    atomic_fetch_add(&pool->free_blocks, 1);
}

//*直接free掉内存池
void fixed_pool_destroy(fixed_size_pool_t* pool) {
    int result = -10;
    if (pool->memory_base) {
        // result = munmap(pool->memory_base, );
        result = munmap(pool->memory_base, pool->actual_mapped_size);
        if (result == -1) {
            switch (errno) {
                case EINVAL:
                    fprintf(stderr, "munmap: invalid arguments\n");
                    break;
                case EPERM:
                    fprintf(stderr, "munmap: permission denied\n");
                    break;
                default:
                    perror("munmap failed");
            }
            fprintf(stderr, "munmap return -1, strerrno: %s\n", strerror(errno));
            exit(-1);
        }
    }
    // pthread_mutex_destroy(&pool->mutex);
    printf("munmap return %d\n", result);
    free(pool);
}

void fixed_pool_stats(fixed_size_pool_t *pool) {
    size_t free_num = atomic_load(&pool->free_blocks);
    size_t sum_num = pool->block_count;
    size_t data_size = pool->block_size ;
    if (pool->free_list) {
        printf("内存池非空，还有空闲块\n");
    }
    printf("内存池总共块个数 -> %lu, 内存池每个数据块大小 -> %lu, 空闲块个数 -> %lu\n", sum_num, data_size, free_num);
}

