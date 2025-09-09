#include "kv_task.h"
#include <errno.h>


extern kv_type_t* g_kv_array;



int is_integer(const char *str) {
    printf("str:%s\n", str);
    if (str == NULL || *str == '\0') {
        printf("不是integer\n");
        return 0;
    }
    
    char *endptr = NULL;
    errno = 0; // 必须重置errno
    long value = strtol(str, &endptr, 10); // 10表示十进制
    printf("errno:%d\n", errno);
    // 三个条件必须同时满足：
    // 1. 整个字符串都被成功转换 (endptr指向字符串结束符)
    // 2. 没有发生溢出错误 (errno保持为0)
    // 3. 至少转换了一个字符 (避免空字符串或纯符号)
    printf("*endptr:%c, errno:%d,endptr:%p\n", *(endptr - 2), errno, endptr);
    return (*endptr == '\0' && errno == 0 && endptr != str);
}

int is_float(const char *str) {
    if (str == NULL || *str == '\0') {
        return 0;
    }
    
    char *endptr; 
    errno = 0;
    double value = strtod(str, &endptr);
    
    // 判断条件与整数类似
    return (*endptr == '\0' && errno == 0 && endptr != str);
}

// 使用宏实现类型安全
//* 宏可以传递类型，更加泛化
#define KV_MALLOC(type, count, ptr) \
    do { \
        void* _temp = malloc(sizeof(type) * (count)); \
        if (_temp == NULL) { \
            printf(" Allocation failed\n"); \
            exit(-1); \
        } \
        *(ptr) = (type*)_temp; \
    } while (0)

// int kv_malloc(uint32_t size, void **p) {
//     if (*p = malloc(size)) {
//         return 0;
//     }
//     printf("kv_malloc failed\n");
//     exit(-1);//*终止整个进程
// }

int kv_free(void *p) {
    free(p);
    return 0;
}

//*返回0找到有效key，没找到返回-1
int KV_GET(char *k, int *pos) {
    printf("3\n");
    printf("GEt key:-> %s\n", k);
    // printf("current key:->%s\n", (char*)c->kv_array[0].key);
    for (int i = 0; i < KV_ARRAY_SIZE; i++) {
        if (g_kv_array[i].key != NULL){
            if (strcmp(g_kv_array[i].key, k) == 0) {
                printf("4\n");
                if (g_kv_array[i].value == NULL) {
                    if(kv_free(g_kv_array[i].key) != 0) {
                        printf("kv_free failed\n");
                        exit(-1);
                    }
                    g_kv_array[i].key = NULL;
                } else {
                    *pos = i;
                    return 0;
                }
            }
        }
    }
    //* 遍历完没有找到有效key，那么返回-1
    return -1;
}

/// @brief 
/// @param c 
/// @param k 
/// @param v 
/// @return 成功返回0，key或者value非法返回-1
int KV_SET(char *k, char *v) {

    //* malloc值到KV_TYPE中，值得不同，类型不同
    //* 分为字符串类型和数值类型，这里数值类型直接用double
    //* 如果要实现INCR，则是原子操作的。
    //* 如果直接用存储字符串，那么为了线程安全就只有使用锁
    //* 而锁是性能瓶颈，原子操作比锁快一个数量级
    //* 那么这里就使用type实现,先从v判断类型是什么
    if (strlen(k) == 0 || strlen(v) == 0) {
        printf("key or value unfair\n");
        return -1;
    }
    if (is_integer(v)) {
        printf("当前请求设置的是integer\n");
        char *endptr = 0;
        long value = strtol(v, &endptr, 10);
        char *rv = 0;
        int pos = -1;
        long *lp = 0;
        KV_MALLOC(long, 1, &lp);
        *lp = value;
        printf("2\n");
        if(KV_GET(k, &pos) == 0) {
            printf("当前key存在,只更改value");
            if (rv != NULL) {
                if(kv_free(rv) != 0) {
                    printf("kv_free failed\n");
                    exit(-1);
                }
            }
            
            // c->kv_array[pos].value = lp;
            // c->kv_array[pos].type = TYPE_INTEGER;
            return 0;
        }
        //* key不存在
        char* sp = 0;//*sp: str_pointer
        printf("SET的KEY不存在");
        KV_MALLOC(char, strlen(k) + 1, &sp);
        printf("4\n");
        memset(sp, 0, strlen(k) + 1);
        printf("4\n");
        strcpy(sp, k);
        printf("sp->:%s\n", sp);
        for (int i = 0; i < KV_ARRAY_SIZE; i++) {
            
            if (g_kv_array[i].key == NULL) {
                g_kv_array[i].key = (void*)sp;
                g_kv_array[i].type = TYPE_INTEGER;
                g_kv_array[i].value = lp;
                printf("键值写入成功,pos: %d\n", i);
                return 0;
            }
        }

        //* 给客户端发送完成信息,在上一层完成send
    } else if (is_float(v)) {
        printf("is_float(v)");
    } else {
        //* 作为普通字符串
        printf("is_common_str");
    }
    //* 未写入到kv_array
    return -1;
}


int KV_DEL(char *k) {
    printf("KV_DEL\n");
}
int KV_INCR(char *k) {
    printf("KV_INCR\n");
}
int KV_DECR(char *k) {
    printf("KV_DECR\n");
}
