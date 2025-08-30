#include "threadpool.h"

thread_pool_t g_thread_pool = {0};

uint8_t Thread_Pool_Init(int num_workers) {
    g_thread_pool.worker_count = num_workers;
    g_thread_pool.workers = (worker_t*)malloc(sizeof(worker_t) * num_workers);
    if (g_thread_pool.workers == NULL) {
        printf("ThreadPool Init Failed!\n");
        exit(-1);
    }
    atomic_store(&g_thread_pool.next_worker_id, 0);
    return THREAD_POOL_SUCCESS ;

}


uint8_t Thread_Pool_Run() {
    pthread_t pid = 0;
    pthread_attr_t attr;

    if (pthread_attr_init(&attr) == 0) {
        if (pthread_attr_setdetachstate(&attr, PTHREAD_CREATE_DETACHED) == 0) {
            for (int i = 0; i < g_thread_pool.worker_count; i++) {
                g_thread_pool.workers[i].worker_id = i;
                atomic_store(&g_thread_pool.workers[i].task_count, 0);
                RB_Init(&g_thread_pool.workers[i].queue, BUFFER_SIZE);
                int ret = pthread_create(&pid, &attr, worker_func, &g_thread_pool.workers[i]);

                if (ret != 0) {
                    pritnf("Thread Create Failed\n");
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

    return NULL;
    // while (1) {
            
    // }
}

