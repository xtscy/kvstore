#include "threadpool.h"

thread_pool_t g_thread_pool = {
    .worker_count  = 0,
    .workers = NULL,
    .scheduler_strategy = NULL,
    .current_queue_num = 0,
    .max_queue_num = MAX_GLOBAL_QUEUE_NUM,
};

uint8_t Thread_Pool_Init(int num_workers) {
    int sign = 0;
    if (g_thread_pool.workers = (worker_t*)malloc(sizeof(worker_t) * num_workers)) {
        if (g_thread_pool.workers == NULL) {
            printf("ThreadPool Init Failed!\n");
            exit(-1);
        }
        for(int i = 0; i < MAX_GLOBAL_QUEUE_NUM; i++) {
            if (LK_RB_Init(&g_thread_pool.global_queue[i], 16) == RING_BUFFER_ERROR) {
                sign = 1;
                break;
            }
        }
        if (!sign) {
            g_thread_pool.worker_count = num_workers;
            g_thread_pool.current_queue_num = MAX_GLOBAL_QUEUE_NUM;//这里先不考虑峰值流量的处理
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


uint8_t Thread_Pool_Run() {
    pthread_t pid = 0;
    pthread_attr_t attr;

    if (pthread_attr_init(&attr) == 0) {
        if (pthread_attr_setdetachstate(&attr, PTHREAD_CREATE_DETACHED) == 0) {
            for (int i = 0; i < g_thread_pool.worker_count; i++) {
                g_thread_pool.workers[i].worker_id = i;
                atomic_store(&g_thread_pool.workers[i].task_count, 0);
                LK_RB_Init(&g_thread_pool.workers[i].queue, BUFFER_SIZE);
                int ret = pthread_create(&pid, &attr, Worker_Func, &g_thread_pool.workers[i]);

                if (ret != 0) {
                    printf("Thread Create Failed\n");
                    exit(-1);
                }
                g_thread_pool.workers[i].thread_id = pid;
            }

            return THREAD_POOL_SUCCESS ;

        }   
        printf("set detached failed\n");
        pthread_attr_destroy(&attr);
        
        return THREAD_POOL_ERROR;
    }
    pthread_attr_destroy(&attr);
    printf("attr init false\n");

    return THREAD_POOL_ERROR ;

}

void* Worker_Func(void* arg) {
    /* 线程入口函数 */
    worker_t * p_worker = (worker_t*)arg;

    printf("run thread tid:%lu, thread wid:%d\n", p_worker->thread_id, p_worker->worker_id);
    
    task_t *task = (task_t*)malloc(sizeof(task_t) * 1);
    //* 这里可以多开几个以实现批处理,1次Read拿到多个task,目前先开1个
    //* 这里task是独属于当前线程的所以，也可以用线程特定变量，和malloc和内存池比性能如何不知道
    if (task == NULL) {
        printf("malloc failed, exit -1");
        exit(-1);
    }

    while (1) {
        //* 未处理的网络消息都会在当前线程的无锁环形队列中
        //* 则去队列取消息，如果没有信息那么返回
        //* 这里线程的队列中的类型是task，所以是一个task一个task的拿然后处理当前的task
        int ret = -1;
        if (LK_RB_Read_Task(&p_worker->queue, task, 1) == RING_BUFFER_SUCCESS) {
            //* 拿到task,然后解析payload，通过payload_len,同时处理完后，直接发送对应消息的数据
            //* 这里是只有body，header在协程框架中已经处理
            //* 这里就是判断是哪个任务，然后执行对应任务即可,这里可以用函数指针
            //* 让协程直接传函数指针给task_t，那么拿到task就可以直接运行
            //* 这里写一个protocal_process协议解析，没有前缀长度的解析
            //* 把字符串解析成单独的token后，再去判断做哪个任务，那么这个设计就和以前的是一摸一样了

            //* 这里直接调用封装函数。
            //* 但是这里如果返回错误怎么办
            //* 这里是从queue中拿任务的，如果当前
            if ((ret = Process_Data_Task(task)) == 0) {
                printf("当前task处理完成\n");
            } else if (ret == -2) {
                printf("当前token没有对应的order\n");
            }
            continue;
        } else {
            //* 当前task拿取失败
            printf("thread:>%d, task 拿取失败,go sleep(5)\n",p_worker->worker_id);
        }
        
        //* 这里还用了任务窃取，但是返回逻辑相同,先把其他的写了再来实现

        // if ()

        //* 最后如果都没有任务，那么这里先用最简单的睡眠
        sleep(5);

    }
}


//* 这里线程池是作为全局参数的，所以可以直接使用，那么直接写函数就行

//* 返回worker线程数组的下标
int Thread_Scheduler(void) {
    //* 先实现最简单的轮询策略
    //* 线程池中有一个int原子变量，就用这个原子变量来实现线程安全的轮询策略
    //* 原子的加该变量，然后返回该值，next_woker_id指向当前应该被用的线程
    //* 这里需要保证一个值只会被拿一次。所以保证的是原子的拿值并且更新
    //* fetch_add一般都是1条指令，用于简单数值增减
    //* 
    //* 这里取模，atomic_uint加到最大值就会溢出然后变为0,符合正确性
    return (atomic_fetch_add(&g_thread_pool.next_worker_id, 1) % g_thread_pool.worker_count);
}
