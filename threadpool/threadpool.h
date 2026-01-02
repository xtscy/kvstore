#ifndef __THREAD_POOL__
#define __THREAD_POOL__
#include <stdio.h>
#include <pthread.h>
#include <stdint.h>
#include <stddef.h>
#include <stdlib.h>
#include <string.h>
#include "../lock_free_ring_buf/lock_free_ring_buf.h"
#include <stdatomic.h>
#include <unistd.h>
#include <semaphore.h>

#define THREAD_POOL_SUCCESS     0x01
#define THREAD_POOL_ERROR       0x00
#define MAX_GLOBAL_QUEUE_NUM    3
#define GLOBAL_TASK_THREADS     3

// Worker线程结构
typedef struct worker_s{
    volatile pthread_t thread_id;           // 线程ID
    int worker_id;                 // Worker的标识符
    // atomic_size_t task_count;     // 当前任务数（用于统计）
    lock_free_ring_buffer queue;  // 每个Worker自己的无锁环形队列
    sem_t sem;

    // wake_sign的初始值设置为1，用于同步阻塞逻辑
} worker_t;

typedef struct global_worker_s{
    volatile pthread_t thread_id;
    int global_id;
    pthread_mutex_t g_mutex;
} global_worker_t;

// 这里可以用一个如果全局任务非常多,但是当前线程任务数少,那么就可以让当前线程去帮忙处理全局队列的任务


// 全局线程池
typedef struct thread_pool_s {
    // 生产者是单线程，消费者是多个线程同时访问global_queue
    // 但是并不会改变全局队列个数和最大队列个数。
    // 但是这里向全局队列push任务，也是轮询，否则第一个队列会很满，而其他队列确没有
    // 这里线程拿任务也是同样的。如果1个线程每次都从第1个任务队列拿
    // 那么其他队列的任务将会有饥饿问题
    // 所以这里使用1个原子变量来记录消费者的轮询
    atomic_uint produce_next_queue_idx;//* 生产者无线程安全
    atomic_uint consume_next_queue_idx;//* 消费者有线程安全问题
    int current_queue_num;  
    int max_queue_num;
    //* 这里先直接在初始化时就去初始化全局队列。
    //* 后续可以不初始化所有全局队列，并且全局队列数量数量可以更多
    //* 当全局队列不够时，可以去动态添加全局队列，直到达到最大值。
    //* 并且在一段时间后，自动清理不需要的全局队列。这样可以有效防止某一时刻或某一小段时间接收到巨量的请求，让服务器阻塞
    lock_free_ring_buffer global_queue[MAX_GLOBAL_QUEUE_NUM];
    worker_t *workers;             // Worker数组
    volatile int worker_count;              // Worker数量
    uint32_t (*scheduler_strategy)(void);
    atomic_uint next_worker_id;    // 用于轮询策略
    
    global_worker_t *global_workers;
    atomic_uint global_pos;
    int global_worker_nums;
    atomic_int sleep_num;// 当前有多少个全局线程正在睡眠
    pthread_cond_t global_cond;
    sem_t sem[MAX_GLOBAL_QUEUE_NUM];
    // 用sleep_num判断是否需要sinal,全局线程睡眠在1个条件变量上

    // 策略函数指针
    //* 这里使用面向对象的思想
} thread_pool_t;


extern uint8_t Thread_Pool_Init(int);
extern void* Worker_Func(void*);
extern uint8_t Thread_Pool_Run();
extern uint32_t Thread_Scheduler(void);
extern thread_pool_t g_thread_pool;
extern int Process_Data_Task(block_alloc_t *t);

#endif