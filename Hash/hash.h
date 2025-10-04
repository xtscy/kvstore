#define _GNU_SOURCE
#include <stdint.h>
#include <stddef.h>
#include "../memory_pool/memory_pool.h"
#include <stdbool.h>
#include <pthread.h>
#include "../kv_task.h"


// 这里用指针指向key起始位置，key从数据池中分配
// 这样就能分配任意大小了
// 这个数据池实现，是没有块封装直接操作字节
typedef struct hash_entry_s {
    size_t key_size;
    size_t value_size;
    value_type_t value_type;
    char kv_data[];
} hash_entry_t;


typedef struct double_hash_table_s {
    //这里使用读写锁
    pthread_rwlock_t rwlock;
    hash_entry_t *table;
    size_t table_size;
    size_t cur_data_size;
    size_t dele_count;
} double_hash_table_t;


extern double_hash_table_t *hash_table_create(size_t initial_size);
extern void hash_table_destroy(double_hash_table_t *table);

// 这里在外部已经判断了值的类型，所以这里可以直接传值大小，不用再去判断值类型了
extern _Bool hash_table_put(double_hash_table_t *table, const char *key, const void *value, int value_size);

extern void* hash_table_get(double_hash_table_t *table, const char *key);
extern bool hash_table_remove(double_hash_table_t *table, const char *key);

extern bool hash_table_contains(double_hash_table_t *table, const char *key);

size_t hash1(const char *key, size_t table_size);
size_t hash2(const char *key, size_t table_size);


void hash_table_cleanup(double_hash_table_t *table);
