#include "threadpool.h"
#include "../memory_pool/memory_pool.h"
#include <time.h>

thread_pool_t g_thread_pool = {
    .worker_count = 0,
    .workers = NULL,
    .scheduler_strategy = NULL,
    .current_queue_num = MAX_GLOBAL_QUEUE_NUM,
    .max_queue_num = MAX_GLOBAL_QUEUE_NUM,
    .global_workers = NULL,
    .global_worker_nums = 0,
};

uint8_t Thread_Pool_Init(int num_workers)
{
    int sign = 0;
    // if ((g_thread_pool.global_workers = (global_worker_t*)malloc(sizeof(global_worker_t) * GLOBAL_TASK_THREADS)) != NULL) {
    //     atomic_init(&g_thread_pool.global_pos, 0); 
    //     g_thread_pool.global_worker_nums = GLOBAL_TASK_THREADS;        
    //     pthread_cond_init(&g_thread_pool.global_cond, NULL);
    //     atomic_init(&g_thread_pool.sleep_num, 0);
    //     for (int i = 0; i < GLOBAL_TASK_THREADS; i++) {
    //         pthread_mutex_init(&g_thread_pool.global_workers[i].g_mutex, NULL);
    //     }
    // } else {
    //     free(g_thread_pool.global_workers);
    //     return THREAD_POOL_ERROR;
    // }

    if ((g_thread_pool.workers = (worker_t *)malloc(sizeof(worker_t) * num_workers)) != NULL)
    {
        if (g_thread_pool.workers == NULL)
        {
            printf("ThreadPool Init Failed!\n");
            abort();
            exit(-1);
        }
        
        // init global queue
        // init global sem
        for (int i = 0; i < g_thread_pool.current_queue_num; i++)
        {
            if (LK_RB_Init(&g_thread_pool.global_queue[i], BUFFER_SIZE) == RING_BUFFER_ERROR)
            {
                sign = 1;
                break;
            }
            if (sem_init(&g_thread_pool.sem[i], 0, 0) == -1) {
                sign = 1;
                break;
            }
        }
        //init worker
        for (int i = 0; i < num_workers; i++) {
            if (sem_init(&g_thread_pool.workers[i].sem, 0, 0) == -1) {
                sign = 1;
                break;
            }
            if (LK_RB_Init(&g_thread_pool.workers[i].queue, BUFFER_SIZE) == RING_BUFFER_ERROR) {
                sign = 1;
                break;
            }
            // atomic_init(&g_thread_pool.task_count, 0);
        }
        if (!sign)
        {
            g_thread_pool.worker_count = num_workers;
            // g_thread_pool.current_queue_num = MAX_GLOBAL_QUEUE_NUM; // 这里先不考虑峰值流量的处理
            g_thread_pool.scheduler_strategy = Thread_Scheduler;
            atomic_init(&g_thread_pool.produce_next_queue_idx, 0);
            atomic_init(&g_thread_pool.consume_next_queue_idx, 0);
            atomic_init(&(g_thread_pool.next_worker_id), 0);
            return THREAD_POOL_SUCCESS;
        }
    }
    free(g_thread_pool.workers);
    return THREAD_POOL_ERROR;
}

static void add_ms_to_time_spec(struct timespec *ts, long ms) {

    ts->tv_sec += ms / 1000;
    ts->tv_nsec += (ms % 1000) * 1000000;

    if (ts->tv_nsec > 1000000000) {
        ts->tv_sec += 1;
        ts->tv_nsec -= 1000000000;
    }
}

static void* Global_Worker_Func(void *arg) {
    global_worker_t *p_worker = (global_worker_t *)arg;
    block_alloc_t *block = (block_alloc_t *)fixed_pool_alloc(global_fixed_pool);
    // printf("Global_Worker_Func 1\n");
    printf("run global_thread tid:%lu, thread wid:%d\n", p_worker->thread_id, p_worker->global_id);
    if (block == NULL) {
        // printf("Global_Worker_Func 2\n");
        printf("malloc failed, exit -1");
        exit(-1);
    }
    // printf("Global_Worker_Func 3\n");
    int ret = -1;
    struct timespec ts = {0};
    uint32_t pos = 0;
    int s_val = -1;
    while (1) {
        // 去全局队列中拿任务处理
        // printf("Global_Worker_Func 4\n");
        pos = atomic_fetch_add_explicit(&g_thread_pool.global_pos, 1, memory_order_acquire);
        // printf("Global_Worker_Func 5\n");

        for (int i = 0; i < g_thread_pool.current_queue_num; i++) {
            // 先用信号量判断当前queue是否有任务
            // printf("Global_Worker_Func 6:%d\n", i);
            
            if (sem_getvalue(&g_thread_pool.sem[pos % g_thread_pool.current_queue_num], &s_val) != 0) {
                perror("sem get value failed\n");
                exit(-4);
            }
            // printf("Global_Worker_Func 7:%d\n", i);

            if (s_val > 0) {
                if (sem_trywait(&g_thread_pool.sem[pos % g_thread_pool.current_queue_num]) == 0) {
                    LK_RB_Read_Block(&g_thread_pool.global_queue[pos], block, 1);
                    if ((ret = Process_Data_Task(block)) == 0) {
                        printf("当前task处理完成\n");
                    }
                    else if (ret == -2) {
                        printf("当前token没有对应的order, 该次中的所有token全部丢弃\n");
                    } else {
                        printf("其他错误\n");
                    }
                    // allocator_deref(block->allocator);
                }
            }
            ++pos;
            // printf("Global_Worker_Func 8:%d\n", i);
        }
        
        // printf("Global_Worker_Func 9\n");
        pthread_mutex_lock(&p_worker->g_mutex);
        // printf("Global_Worker_Func 10\n");

        // printf("Global_Worker_Func 11\n");
        clock_gettime(CLOCK_REALTIME, &ts);
        // printf("Global_Worker_Func 12\n");
        add_ms_to_time_spec(&ts, 1755);
        // printf("Global_Worker_Func 13\n");
        
        atomic_fetch_add_explicit(&g_thread_pool.sleep_num, 1, memory_order_release);
        // printf("Global_Worker_Func 14\n");

        pthread_cond_timedwait(&g_thread_pool.global_cond, &p_worker->g_mutex, &ts);
        // printf("Global_Worker_Func 15\n");

        atomic_fetch_sub_explicit(&g_thread_pool.sleep_num, 1, memory_order_release);
        pthread_mutex_unlock(&p_worker->g_mutex);
    }
    
}  

uint8_t Thread_Pool_Run()
{
    pthread_t pid = 0;
    pthread_attr_t attr;

    // pthread_attr_t global_attr;
    // if (pthread_attr_init(&global_attr) == 0) {
    //     if (pthread_attr_setdetachstate(&global_attr, PTHREAD_CREATE_DETACHED) == 0) {
    //         for (int i = 0; i < 1; i++) {
    //             printf("GLOBAL_TASK_THREADS : %d\n", GLOBAL_TASK_THREADS);
    //             g_thread_pool.global_workers[i].global_id = 1000 + i;
    //             // pthread_mutex_init(&g_thread_pool.global_workers[i].g_mutex, NULL);
    //             int ret = pthread_create(&pid, &global_attr, Global_Worker_Func, &g_thread_pool.global_workers[i]);
    //             g_thread_pool.global_workers[i].thread_id = pid;
    //             if (ret != 0) {
    //                 printf("global thread create failed\n");
    //                 exit(-1);
    //             }
    //         }
    //     }
    // } else {
    //     return THREAD_POOL_ERROR;
    // }
    
    if (pthread_attr_init(&attr) == 0)
    {
        if (pthread_attr_setdetachstate(&attr, PTHREAD_CREATE_DETACHED) == 0)
        {
            for (int i = 0; i < g_thread_pool.worker_count; i++)
            {
                g_thread_pool.workers[i].worker_id = i;
                // atomic_store(&g_thread_pool.workers[i].task_count, 0);
                // LK_RB_Init(&g_thread_pool.workers[i].queue, BUFFER_SIZE);
                int ret = pthread_create(&pid, &attr, Worker_Func, &g_thread_pool.workers[i]);

                if (ret != 0)
                {
                    printf("Thread Create Failed\n");
                    abort();
                    exit(-1);
                }
                g_thread_pool.workers[i].thread_id = pid;
            }

            return THREAD_POOL_SUCCESS;
        }
        printf("set detached failed\n");
        pthread_attr_destroy(&attr);

        return THREAD_POOL_ERROR;
    }
    pthread_attr_destroy(&attr);
    printf("attr init false\n");

    return THREAD_POOL_ERROR;
}

void *Worker_Func(void *arg)
{
    /*
    线程入口函数 */
    worker_t *p_worker = (worker_t *)arg;

    printf("run thread tid:%lu, thread wid:%d\n", p_worker->thread_id, p_worker->worker_id);

    block_alloc_t *block = (block_alloc_t *)fixed_pool_alloc(global_fixed_pool);
    // block_alloc_t *block = (block_alloc_t*)malloc(sizeof(block_alloc_t) * 1);
    //* 这里可以多开几个以实现批处理,1次Read拿到多个task,目前先开1个
    //* 这里task是独属于当前线程的所以，也可以用线程特定变量，和malloc和内存池比性能如何不知道
    if (block == NULL)
    {
        printf("malloc failed, exit -1");
        abort();
        exit(-1);
    }

    while (1)
    {
        //* 未处理的网络消息都会在当前线程的无锁环形队列中
        //* 则去队列取消息，如果没有信息那么返回
        //* 这里线程的队列中的类型是task，所以是一个task一个task的拿然后处理当前的task
        int ret = -1;
        if(sem_wait(&p_worker->sem) == 0) {
            if (LK_RB_Read_Block(&p_worker->queue, block, 1U) == RING_BUFFER_ERROR) {
                printf("LK_RB_Read_Block failed\n");
                int sem_cur_val = 0;
                sem_getvalue(&p_worker->sem, &sem_cur_val);
                printf("sem val%d\n", sem_cur_val);
                abort();
            }

            printf("worke->block->%s,worker->block->size%lu\n", block->ptr, block->size);
            
            if ((ret = Process_Data_Task(block)) == 0) {
                printf("当前block处理完成\n");
            } else if (ret == -2) {
                printf("当前token没有对应的order, 该次中的所有token全部丢弃\n");
                abort();
            } else {
                printf("其他错误\n");
                abort();
            }
            // allocator_deref(block->allocator);
            // fixed_pool_free(global_fixed_pool, )
            continue; 
        }
    }
}

//* 这里线程池是作为全局参数的，所以可以直接使用，那么直接写函数就行

//* 返回worker线程数组的下标
uint32_t Thread_Scheduler(void)
{
    return (atomic_fetch_add_explicit(&g_thread_pool.next_worker_id, 1, memory_order_relaxed) % g_thread_pool.worker_count);
}
