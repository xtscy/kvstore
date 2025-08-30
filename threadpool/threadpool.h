// Worker线程结构
#include <pthread.h>
#include <stdint.h>
#include <stddef.h>
#include <stdlib.h>
#include <string.h>
#include "../lock_free_ring_buf/lock_free_ring_buf.h"
#include <stdatomic.h>

#define THREAD_POOL_SUCCESS     0x01
#define THREAD_POOL_ERROR       0x00


typedef struct {
    pthread_t thread_id;           // 线程ID
    int worker_id;                 // Worker的标识符
    _Atomic(size_t) task_count;     // 当前任务数（用于统计）
    lock_free_ring_buffer queue;  // 每个Worker自己的无锁环形队列
} worker_t;

// 全局线程池
typedef struct {
    worker_t *workers;             // Worker数组
    int worker_count;              // Worker数量
    _Atomic(int) next_worker_id;    // 用于轮询策略
    // 策略函数指针
    int (*scheduler_strategy)(struct thread_pool_t*);
} thread_pool_t;


extern uint8_t Thread_Pool_Init(int);
extern void* Worker_Func(void*);
extern uint8_t Thread_Pool_Run();