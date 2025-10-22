
#define _GNU_SOURCE

#include <stdio.h>
#include <math.h>
#include <stdlib.h>
#include <stdint.h>
#include <stdbool.h>
#include <string.h>


typedef enum value_type_e {
    Unknow,
    Int,
    Double,
    String,
} value_type_t;


typedef struct hash_item_s {

    char *key;
    void *val;
    uint16_t key_size;// 不计算前导和后导0
    uint16_t val_size;
    bool is_occupied;
    bool is_deleted;
    value_type_t val_type;

} hash_item_t;

typedef struct hash_table_s {
    hash_item_t *table;
    size_t size;

} hash_table_t;

extern hash_table_t* hash_table_create(size_t, hash_table_t*);
extern void destroy_table(hash_table_t*);

extern unsigned long hash_init_pos(unsigned long, hash_table_t*);
extern unsigned long hash_step_size(unsigned long, hash_table_t*);
extern bool insert(unsigned long, void*, uint16_t, void*, uint16_t, hash_table_t*, value_type_t);

extern unsigned long string_to_key(const char*);
extern bool key_compare(hash_item_t*, void*, uint16_t);

extern void* hash_table_search(unsigned long, void*, uint16_t, hash_table_t*);

extern long hash_table_search_int(char*, uint16_t, hash_table_t*);
extern double hash_table_search_double(char*, uint16_t, hash_table_t*);
extern char* hash_table_search_string(char*,uint16_t, uint16_t*, hash_table_t*);

