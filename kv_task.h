#ifndef _KV_TASK_
#define _KV_TASK_

#include <stdint.h>
#include <stddef.h>
#include <stdio.h>
#include <string.h>
#include "RingBuffer/ring_buffer.h"
#include "./lock_free_ring_buf/lock_free_ring_buf.h"
#include "memory_pool/memory_pool.h"

#define READ_CACHE_SIZE 1024*16
#define RING_BUF_SIZE 1000
#define KV_ARRAY_SIZE 1000



typedef uint32_t message_header_t;


typedef enum value_type_e{
    TYPE_UNKNOWN,
    TYPE_INTEGER,
    TYPE_FLOAT, 
    TYPE_STRING
} value_type_t;

typedef struct {
    char cache[READ_CACHE_SIZE];
    uint32_t length;
    uint32_t head;
} read_cache_t;

typedef enum {
    PARSE_STATE_HEADER,
    PARSE_STATE_BODY
} read_state_t;

// typedef struct kv_type_s{
//     //* 这里用指针，然后直接在堆上存放数据
//     void* key;
//     void* value;
//     value_type_t type;
// } kv_type_t;

// 对缓存更友好
// 键和值都存储到柔性数组data中
typedef struct {
    value_type_t type;
    uint16_t key_len;
    uint16_t value_len;
    char data[];  // 柔性数组，存储key+value
} kv_type_t;

// 只需要一个支持变长分配的内存池

typedef struct connection_s {
    int fd;
    //* 这里使用全局数据结构
    // kv_type kv_array[KV_ARRAY_SIZE];//* 存储键值对数据的数组
    // char* tokens[16];//* 存储解析到的token
    // char pkt_data[RING_BUF_SIZE + 1];//* 多一个终止符
    //* 这里只需要放入read_rb.然后放入线程的任务队列中
    ring_buffer read_rb;     // 读环形缓冲区，在该缓冲区处理包  
    read_cache_t read_cache;   // 数据缓冲区，数据最先放在read_cache
    read_state_t state;
    message_header_t current_header;
    
} connection_t;
// enum value_type_e;
typedef enum value_type_e value_type_t;


// extern struct task_s;
// typedef struct task_s task_t;

extern int KV_SET(char *k, char *v);
extern int KV_GET(char *k, int *pos);
extern int KV_DEL(char *k);
extern int KV_INCR(char *k);
extern int KV_DECR(char *k);
// extern int Process_Task(connection_t *c);
extern int Process_Data_Task(task_t *);
extern int token_to_order(char *buf);
#endif