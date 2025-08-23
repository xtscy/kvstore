#include <stdint.h>
#include <stddef.h>
#include <stdio.h>
#include <string.h>
#include "RingBuffer/ring_buffer.h"


#define READ_CACHE_SIZE 1024*16
#define RING_BUF_SIZE 8192
#define KV_ARRAY_SIZE 100


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

typedef struct {
    //* 这里用指针，然后直接在堆上存放数据
    void* key;
    void* value;
    value_type_t type;
} kv_type;


typedef struct connection_s{
    int fd;
    kv_type kv_array[KV_ARRAY_SIZE];//* 存储键值对数据的数组
    char* tokens[16];//* 存储解析到的token
    char pkt_data[RING_BUF_SIZE + 1];//* 多一个终止符
    ring_buffer read_rb;     // 读环形缓冲区，在该缓冲区处理包
    read_cache_t read_cache;   // 数据缓冲区，数据最先放在read_cache
    read_state_t state;
    message_header_t current_header;
    
} connection_t;
enum value_type_e;
typedef enum value_type_e value_type_t;


extern int KV_SET(connection_t *c, char *k, char *v);
extern int KV_GET(connection_t *c, char *k, int *pos);
extern int KV_DEL(connection_t *c, char *k);
extern int KV_INCR(connection_t *c, char *k);
extern int KV_DECR(connection_t *c, char *k);
extern int Process_Task(connection_t *c);