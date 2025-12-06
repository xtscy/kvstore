#include "threadpool.h"
#include "../memory_pool/memory_pool.h"
#include <time.h>

thread_pool_t g_thread_pool = {
    .worker_count = 0,
    .workers = NULL,
    .scheduler_strategy = NULL,
    .current_queue_num = 0,
    .max_queue_num = MAX_GLOBAL_QUEUE_NUM,
    .global_workers = NULL,
    .global_worker_nums = 0,
};

uint8_t Thread_Pool_Init(int num_workers)
{
    int sign = 0;
    if (g_thread_pool.global_workers = (worker_t *)malloc(sizeof(global_worker_t) * GLOBAL_TASK_THREADS) {

        g_thread_pool.global_worker_nums = GLOBAL_TASK_THREADS;        
        pthread_cond_init(&g_thread_pool.global_cond, NULL);
    } else {
        free(g_thread_pool.global_workers);
        return THREAD_POOL_ERROR;
    }

    if (g_thread_pool.workers = (worker_t *)malloc(sizeof(worker_t) * num_workers)
    {
        if (g_thread_pool.workers == NULL)
        {
            printf("ThreadPool Init Failed!\n");
            exit(-1);
        }
        for (int i = 0; i < MAX_GLOBAL_QUEUE_NUM; i++)
        {
            if (LK_RB_Init(&g_thread_pool.global_queue[i], 16) == RING_BUFFER_ERROR)
            {
                sign = 1;
                break;
            }
        }
        if (!sign)
        {
            g_thread_pool.worker_count = num_workers;
            g_thread_pool.current_queue_num = MAX_GLOBAL_QUEUE_NUM; // 这里先不考虑峰值流量的处理
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
    global_worker_t *p_worker = (worker_t *)arg;
    block_alloc_t *block = (block_alloc_t *)fixed_pool_alloc(global_fixed_pool);
    printf("run global_thread tid:%lu, thread wid:%d\n", p_worker->thread_id, p_worker->worker_id);
    if (block == NULL) {
        printf("malloc failed, exit -1");
        exit(-1);
    }

    int ret = -1;
    struct timespec ts = {0};
    int pos = -1;
    while (1) {
        // 去全局队列中拿任务处理
        pos = atomic_fetch_add_explicit(&g_thread_pool.global_pos, 1, memory_order_acquire);
        for (int i = 0; i < g_thread_pool.max_queue_num; i++) {
            // 先用信号量判断当前queue是否有任务
            if (sem_getvalue(g_thread_pool.sem[pos]) > 0) {
                if (sem_trywait(g_thread_pool.sem[pos]) == 0) {
                    LK_RB_Read_Task(&g_thread_pool.global_queue[pos], block, 1);
                    if ((ret = Process_Data_Task(block)) == 0) {
                        printf("当前task处理完成\n");
                    }
                    else if (ret == -2) {
                        printf("当前token没有对应的order, 该次中的所有token全部丢弃\n");
                    }
                    allocator_deref(block->allocator);
                }
            }
            ++pos;
        }

        pthread_mutex_lock(&p_worker->g_mutex);

        clock_gettime(CLOCK_REALTIME, &ts);
        add_ms_to_time_spec(&ts, 755);
        
        atomic_fetch_add_explicit(&g_thread_pool.sleep_num, 1, memory_order_release);
        pthread_cond_timedwait(&g_thread_pool.global_cond, &p_worker->g_mutex, &ts);
        atomic_fetch_sub_explicit(&g_thread_pool.sleep_num, 1, memory_order_release);
    }
    
}  

uint8_t Thread_Pool_Run()
{
    pthread_t pid = 0;
    pthread_attr_t attr;

    pthread_attr_t global_attr;
    if (pthread_attr_init(&global_attr) == 0) {
        if (pthread_attr_setdetachstate(&global_attr, PTHREAD_CREATE_DETACHED) == 0) {
            for (int i = 0; i < GLOBAL_TASK_THREADS; i++) {
                g_thread_pool.global_workers[i].global_id = 1000 + i;
                pthread_mutex_init(&g_thread_pool.global_workers[i].g_mutex, NULL);
                int ret = pthread_create(&pid, &global_attr, Global_Worker_Func, &g_thread_pool.global_workers[i]);
                g_thread_pool.global_workers[i].thread_id = pid;
                if (ret != 0) {
                    printf("global thread create failed\n");
                    exit(-1);
                }
            }
        }
    } else {
        return THREAD_POOL_ERROR;
    }
    
    if (pthread_attr_init(&attr) == 0)
    {
        if (pthread_attr_setdetachstate(&attr, PTHREAD_CREATE_DETACHED) == 0)
        {
            for (int i = 0; i < g_thread_pool.worker_count; i++)
            {
                g_thread_pool.workers[i].worker_id = i;
                atomic_store(&g_thread_pool.workers[i].task_count, 0);
                LK_RB_Init(&g_thread_pool.workers[i].queue, BUFFER_SIZE);
                pthread_cond_init(&g_thread_pool.workers[i].cond, NULL);
                pthread_mutex_init(&g_thread_pool.workers[i].mutex, NULL);
                int ret = pthread_create(&pid, &attr, Worker_Func, &g_thread_pool.workers[i]);

                if (ret != 0)
                {
                    printf("Thread Create Failed\n");
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
        exit(-1);
    }

    while (1)
    {
        //* 未处理的网络消息都会在当前线程的无锁环形队列中
        //* 则去队列取消息，如果没有信息那么返回
        //* 这里线程的队列中的类型是task，所以是一个task一个task的拿然后处理当前的task
        int ret = -1;
        if(sem_wait(p_worker->sem) == 0) {
            LK_RB_Read_Task(&p_worker->queue, block, 1);
            //* 拿到task,然后解析payload，通过payload_len,同时处理完后，直接发送对应消息的数据
            //* 这里是只有body，header在协程框架中已经处理
            //* 这里就是判断是哪个任务，然后执行对应任务即可,这里可以用函数指针
            //* 让协程直接传函数指针给task_t，那么拿到task就可以直接运行
            //* 这里写一个protocal_process协议解析，没有前缀长度的解析
            //* 把字符串解析成单独的token后，再去判断做哪个任务，那么这个设计就和以前的是一摸一样了

            if ((ret = Process_Data_Task(block)) == 0) {
                printf("当前block处理完成\n");
            } else if (ret == -2) {
                printf("当前token没有对应的order, 该次中的所有token全部丢弃\n");
            }
            allocator_deref(block->allocator);
            // fixed_pool_free(global_fixed_pool, )
            continue; 
        }
    }
        // else {
        //     // 去遍历全局队列
        //     for (int i = 0; i < g_thread_pool.max_queue_num; i++)
        //     {
        //         if (sem_trywait(&g_thread_pool.global_queue[i]) == 0) {
        //             LK_RB_Read_Task(&g_thread_pool.global_queue[i], block, 1);
        //             if ((ret = Process_Data_Task(block)) == 0)
        //             {
        //                 printf("当前task处理完成\n");
        //             }
        //             else if (ret == -2)
        //             {
        //                 printf("当前token没有对应的order, 该次中的所有token全部丢弃\n");
        //             }
        //             allocator_deref(block->allocator);
        //             continue;
        //         }
        //     }
        // }
        // sem_wait(&g_thread_pool.wake_sign);
        
        // }else if (LK_RB_Read_Task(&p_worker->queue, block, 1) == RING_BUFFER_SUCCESS) {
        //     g_thread_pool
        // } else {
        //* 当前task拿取失败
        //* 这里用条件变量等待
        // pthread_cond_wait(p_worker->cond,, p_worker->mutex);
        // printf("thread:>%d, task 拿取失败,go sleep(5)\n",p_worker->worker_id);
        // }
    // }
}

//* 这里线程池是作为全局参数的，所以可以直接使用，那么直接写函数就行

//* 返回worker线程数组的下标
int Thread_Scheduler(void)
{
    //* 先实现最简单的轮询策略
    //* 线程池中有一个int原子变量，就用这个原子变量来实现线程安全的轮询策略
    //* 原子的加该变量，然后返回该值，next_woker_id指向当前应该被用的线程
    //* 这里需要保证一个值只会被拿一次。所以保证的是原子的拿值并且更新
    //* fetch_add一般都是1条指令，用于简单数值增减
    //*
    //* 这里取模，atomic_uint加到最大值就会溢出然后变为0,符合正确性
    return (atomic_fetch_add(&g_thread_pool.next_worker_id, 1) % g_thread_pool.worker_count);
}
