#ifndef _KV_TASK_
#define _KV_TASK_

#include <stdint.h>
#include <stddef.h>
#include <stdio.h>
#include <string.h>
#include "RingBuffer/ring_buffer.h"
#include "./lock_free_ring_buf/lock_free_ring_buf.h"
#include "memory_pool/memory_pool.h"
#include "./MemoryBTreeLock/m_btree.h"
#include "./resp_state_stack/resp_state_stack.h"

#define READ_CACHE_SIZE 1024*16
#define RING_BUF_SIZE 1024
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
    //* 这里只需要放入read_rb.然后放入线程的任务队列中
    
    int fd;

    ring_buffer read_rb;     // 读环形缓冲区，在该缓冲区处理包  
    read_cache_t read_cache;   // 数据缓冲区，数据最先放在read_cache
    read_state_t state;
    resp_state_stack_t parser_stack;




    // 连接状态, 后续实现连接池
    // uint32_t state;
    // #define CONN_STATE_READING   0x01
    // #define CONN_STATE_PARSING   0x02
    // #define CONN_STATE_PROCESSING 0x04
    // #define CONN_STATE_WRITING   0x08

     /*** 当前对象信息 ***/
    // struct {
    //     char type;              // 当前解析的类型: '+', '-', ':', '$', '*'
        
    //     // 对于简单类型（+ - :）的行数据累积
    //     struct {
    //         char buffer[256];   // 行数据缓冲区（固定大小）
    //         size_t pos;         // 当前位置
    //         size_t expected_end;// 期望结束位置（如果有长度的话）
    //     } line;
        
    //     // 对于批量字符串（$）的数据累积
    //     struct {
    //         // 两种存储策略：
    //         union {
    //             char inline_data[128];  // 小数据：内联存储
    //             char* heap_data;        // 大数据：堆存储
    //         } storage;
            
    //         size_t allocated;   // 已分配大小（堆存储时）
    //         size_t filled;      // 已填充大小
    //         size_t expected;    // 期望总大小
    //         uint8_t storage_type; // 0=内联, 1=堆
    //     } bulk;
        
    //     // 对于数组（*）的累积
    //     struct {
    //         long long expected_count;    // 期望元素个数
    //         long long parsed_count;      // 已解析元素个数
            
    //         // 已解析的元素暂存
    //         void** elements;             // 元素指针数组
    //         size_t elements_capacity;    // 数组容量
            
    //         // 当前正在解析的元素状态
    //         struct IncompleteParser* current_element;
    //     } array;
        
    //     // 对于所有类型的通用信息
    //     long long length_value;          // 从长度行解析出的值
    //     size_t total_bytes_needed;       // 总共需要的字节数
    //     size_t bytes_received;           // 已接收的字节数
    // } current;

    //     /*** 父对象引用（用于嵌套） ***/
    //     //* 用于数组的嵌套请求
    //     //* 这里用链式和循环,来模拟递归的栈开销
    // struct IncompleteParser* parent;
    
    // /*** 错误状态 ***/
    // int has_error;
    // char error_msg[128];
    
    // message_header_t current_header;
    
} connection_t;



// enum value_type_e;
typedef enum value_type_e value_type_t;


// extern struct task_s;
// typedef struct task_s task_t;

extern int KV_SET(char *k, char *v);
extern int KV_GET(char *k, int* val);
extern int KV_DEL(char *k);
extern int KV_INCR(char *k);
extern int KV_DECR(char *k);
extern int KV_SORT(char*, char**);
// extern int Process_Task(connection_t *c);
extern int Process_Data_Task(block_alloc_t *block);
extern int token_to_order(char *buf);
#endif