#include "hash.h"

double_hash_table_t *hash_table = NULL;

fixed_size_pool_t *hash_pool = NULL;
fixed_size_pool_t *handle_pool = NULL;
// 在入口函数初始化handle_pool

// 然后直接在hash_create中直接向handle_pool申请
double_hash_table_t *hash_table = NULL;

double_hash_table_t *hash_table_create(size_t initial_size) {
    // 先向内存池申请存储table_t表结构，然后再申请hash_entry_t存储kv数据，这里连续向内存池申请，具有缓存连续性
    // initial_size是kv_data数组的个数
    // 从句柄池中申请块来存放table_t
    hash_table = fixed_pool_alloc(handle_pool);
    // 然后初始化hash_table
    //! 这里应该也不用加锁，后续再看，先加锁
    pthread_rwlock_wrlock(&hash_table->rwlock);
    hash_table->table_size = initial_size;
    hash_table->cur_data_size = 0;
    hash_table->dele_count = 0;
    //在锁里面申请内存
    pthread_rwlock_unlock(&hash_table->rwlock);
    // 这里块大小和块个数
    // 块个数就是initial_size
    
    fixed_pool_create()
    

}
 
void hash_table_destroy(double_hash_table_t *table) {

}

_Bool hash_table_put(double_hash_table_t *table, const char *key, const void *value, int value_size) {

}


void *hash_table_get(double_hash_table_t *table, const char *key) {

}

bool hash_table_remove(double_hash_table_t *table, const char *key) {

}

bool hash_table_contains(double_hash_table_t *table, const char *key) {

}

size_t hash1(const char *key, size_t table_size) {

}

size_t hash2(const char *key, size_t table_size) {

}


void hash_table_cleanup(double_hash_table_t *table) {

}

