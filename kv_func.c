#include "kv_task.h"
#include <errno.h>
#include "BPlusTree/bpt_c.h"
#include "Persister/persister_c.h"


extern persister_handle global_persister;

int is_integer(const char *str) {
    printf("str:%s\n", str);
    if (str == NULL || *str == '\0') {
        printf("不是integer\n");
        return 0;
    }
    
    char *endptr = NULL;
    errno = 0; // 必须重置errno
    long value = strtol(str, &endptr, 10); // 10表示十进制
    // printf("errno:%d\n", errno);
    // 三个条件必须同时满足：
    // 1. 整个字符串都被成功转换 (endptr指向字符串结束符)
    // 2. 没有发生溢出错误 (errno保持为0)
    // 3. 至少转换了一个字符 (避免空字符串或纯符号)
    // printf("*endptr:%c, errno:%d,endptr:%p\n", *(endptr - 2), errno, endptr);
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


extern volatile bool stage;
//*返回0找到有效key，没找到返回-1
// 这里使用内存池，那么就没有数组访问，而是使用alloc申请块，然后放入块中
// 这里GET某个key，也是去到内存池中查找
int KV_GET(char *key, int* val) {
    printf("3\n");
    printf("GEt key:-> %s\n", key);
    // int ret = btree_search_c(global_bplus_tree, key, val);
    bkey_t bkey = {0};
    memcpy(bkey.key, key, strlen(key));
    search_result_t ret = btree_search(global_m_btree, &bkey);
    if (ret.found == true) {// 从机和主机都可以get,但是只有主机需要持久化
        *val = *(int*)bkey.data_ptrs;
        if (stage == true) {
            persister_get(global_persister, key);
        }
        return 0;
    } else return -1;
    // printf("current key:->%s\n", (char*)c->kv_array[0].key);
    // for (int i = 0; i < KV_ARRAY_SIZE; i++) {
    //     if (g_kv_array[i].key != NULL){
    //         if (strcmp(g_kv_array[i].key, k) == 0) {
    //             printf("4\n");
    //             if (g_kv_array[i].value == NULL) {
    //                 if(kv_free(g_kv_array[i].key) != 0) {
    //                     printf("kv_free failed\n");
    //                     exit(-1);
    //                 }
    //                 g_kv_array[i].key = NULL;
    //             } else {
    //                 *pos = i;
    //                 return 0;
    //             }
    //         }
    //     }
    // }
    //* 遍历完没有找到有效key，那么返回-1
    // return -1;/
}

/// @brief 
/// @param c 
/// @param k 
/// @param v 
/// @return 成功返回0，key或者value非法返回-1

// 向内存的b树进行增添
// 当满足一定条件时去全量数据
// 这里使用b+树向磁盘上写入
// 每次全量后，删除旧的增量文件
// 这里全量时，使用上一次全量文件 + 增量文件来获得新的全量
// 即全量时才用b+树写入，普通写入直接写到内存中
// 这里全量写入可以用另外一个线程，这里是否需要遍历内存的b树呢？不需要
// 只需要遍历增量文件。在写全量时需要用到增量文件，其他修改操作也会用到增量文件
// 所以这里写到磁盘时，需要阻塞其他操作。在写入磁盘，删除旧增量文件后，再去执行操作，把操作继续写到当前增量文件中
// 即程序启动，读取磁盘文件通过遍历b+树把值写入内存b树，然后后续的更改都写入增量文件
// 并且这里采取文件大小来分割增量文件，然后再给一个控制台线程，用于输入退出
// 退出时把增量文件写到全量文件中

// 那么这里就是向内存中写，这里读操作就是在内存中读
// 且保存数据修改操作，而不是读取操作到增量文件,增量文件达到一定大小生成新的增量文件
// 名字后缀用一个数字序号标识顺序.最初始为1，这里打开文件，如果打开失败说明增量文件全部读取完成
// 每过一段时间就生成新的全量文件，在控制台退出时也会阻塞所有操作，然后把增量文件全部刷新到全量文件中

// 那么这里写入的操作肯定要有某种协议格式，才能正确解析增量文件中的保存的操作
// 1:set 2:remove
// 1 key1 value1\r\n1 key2 value2\r\n
// 这里可以写二进制信息，也可以写字符，这里为了便于观察可以直接写字符信息，这样可以直接查看

// 启动时如果没有全量则没有数据需要同步，然后直接增量最后增量去创建全量
// 如果写入操作时也不存在增量文件，那么从1后缀开始创建，并且记录当前序号，以便于递增序号创建下一个

// 启动时如果存在全量那么就做数据同步
// 写入操作时如果存在增量文件，那么尝试写入，如果大小超过了阈值，那么生成新的增量文件
// 让指向增量文件的指针指向新的增量文件，然后写入数据

// 这里呢就先进行内存操作，内存操作完以后再写入到文件中
// 这里数据的处理也是多线程，那么对于同一个文件，需要某种保护机制，
// 这里是线程间的并发写入，多个任务线程处理任务，对同一个区域写入数据
// 那么就会对同一个增量文件进行写入，这里是不同的fd也写入到同一个文件
// 相同的fd也写到同一个文件
// 那么这里可以先写一个全局的增量文件对象，然后在每个任务都使用同一个对象来进行写入
// 由于是并发，这里需要适当的同步机制，否则对同一个文件进行写入则会有问题
// 最简单就是使用互斥锁，这里就直接使用互斥锁

// 

int KV_SORT(char *cnt, char **target) {

    int num = atoi(cnt);
    if (num <= 0) {
        *target = (char*)malloc(16);
        return -1;
    }

    pthread_rwlock_wrlock(&global_m_btree->rwlock);
	btree_iterator_t *iterator = create_iterator(global_m_btree);
	// char buf[64] = {0};
	// int used = 0;
    int current_size = 32;
    *target = (char*)calloc(current_size, 1);
    if (*target == NULL) {
        printf("target calloc NULL\n");
        abort();
    }
    int cur_pos = 0;
    int total = 0;
    int old_num = num;
	while (iterator->current->state != ITER_STATE_END && num > 0) {
        
		bkey_t key_val = iterator_get(iterator);
		printf("获取内存的b树的键值,key:%s,val:%d\n", key_val.key, *(int*)key_val.data_ptrs);
        total += snprintf(NULL, 0, "%d ", *(int*)key_val.data_ptrs);
        if (total < current_size) {
            cur_pos += snprintf(*target + cur_pos, INT16_MAX, "%d ", *(int*)key_val.data_ptrs);
        } else {
            printf("total:%d",total);
            int temp_val = current_size;
            while (temp_val <= total) {
                temp_val *= 2;
            }
            char* temp = malloc(temp_val);
            memset(temp, 0, current_size);
            current_size = temp_val;
            if (temp == NULL) {
                printf("开辟失败\n");
                abort();
            }
            memcpy(temp, *target, cur_pos);
            free(*target);
            *target = temp;
            cur_pos += snprintf(*target + cur_pos, INT16_MAX, "%d ", *(int*)key_val.data_ptrs);
        }
        iterator_find_next(iterator);
        num--;
    }
    pthread_rwlock_unlock(&global_m_btree->rwlock);
    if (old_num == num) {
        return -2;
    } else {
        return 0;
    }
}
 
int KV_SET(char *key, char *val) {


    if (strlen(key) == 0 || strlen(val) == 0) {
        printf("key or value unfair\n");
        exit(-2);
    }
    // 这里向bplustree中写入数据，调用bplus接口,直接向磁盘写入
    if (is_integer(val)) {
        printf("当前请求设置的是integer\n");
        char *endptr = 0;
        // 这里val值不超过int由传参保证
        int value = (int)strtol(val, &endptr, 10);
        // char *rv = 0;
        int pos = -1;
        // long *lp = 0;
        // 写内存
        void *dptr = fixed_pool_alloc(int_global_fixed_pool);
        *((int*)dptr) = value;
        bkey_t temp_key = {0};
        temp_key.data_ptrs = dptr;
        sprintf(temp_key.key, "%s", key);
        // 写入内存
        // 写入增量文件
        // 1 key1 value1\r\n1 key2 value2\r\n
        // 调用封装的c接口的insert操作        
        btree_insert(global_m_btree, temp_key, int_global_fixed_pool);
        if (stage == true) {
            // 目前还没有实现从机的insert操作，后续可实现
            // 每个从机同时连接到主机的task_port,然后把insert转发给主机
            // 然后由主机来把请求转发给所有的从机，从机处理主机的请求
            // 这里需要再写一个从机接收外部连接发送的命令然后转发给主机任务端口的线程
            persister_insert(global_persister, temp_key.key, *((int*)temp_key.data_ptrs));
        }

        
        
        // btree_insert_c(global_bplus_tree, key, value);
        // btree_insert_c(global_bplus_tree)
        // KV_MALLOC(long, 1, &lp);
        // *lp = value;
        // printf("2\n");
        return 0;
        // if(KV_GET(k, &pos) == 0) {
        //     printf("当前key存在,只更改value");
        //     if (rv != NULL) {
        //         if(kv_free(rv) != 0) {
        //             printf("kv_free failed\n");
        //             exit(-1);
        //         }
        //     }
            
        //     // c->kv_array[pos].value = lp;
        //     // c->kv_array[pos].type = TYPE_INTEGER;
        //     return 0;
        // }
        //* key不存在
        // char* sp = 0;//*sp: str_pointer
        // printf("SET的KEY不存在");
        // KV_MALLOC(char, strlen(k) + 1, &sp);
        // printf("4\n");
        // memset(sp, 0, strlen(k) + 1);
        // printf("4\n");
        // strcpy(sp, k);
        // printf("sp->:%s\n", sp);
        // for (int i = 0; i < KV_ARRAY_SIZE; i++) {
            
        //     if (g_kv_array[i].key == NULL) {
        //         g_kv_array[i].key = (void*)sp;
        //         g_kv_array[i].type = TYPE_INTEGER;
        //         g_kv_array[i].value = lp;
        //         printf("键值写入成功,pos: %d\n", i);
        //         return 0;
        //     }
        // }

        //* 给客户端发送完成信息,在上一层完成send
    } else if (is_float(val)) {
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
