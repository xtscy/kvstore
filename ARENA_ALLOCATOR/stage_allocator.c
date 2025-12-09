

#include "stage_allocator.h"
#include <pthread.h>


#define Threshold_Deref   500


int alloc_count = 0;

allocator_out_t global_allocator = {0};



bool allocator_out_init(allocator_out_t *o_allocator) {

    if (o_allocator == NULL) {
        printf("init out allocator falied\n");
        exit(-1);
    }
    
    long logic_cpu = sysconf(_SC_NPROCESSORS_ONLN);
    atomic_init(&o_allocator->head, NULL);
    atomic_init(&o_allocator->current, NULL);
    stage_allocator_t *temp = NULL;
    temp = malloc(sizeof(stage_allocator_t) * logic_cpu);
    for (int i = 0; i < logic_cpu; i++) {
        temp[i].stage = stage_create(stage_size);
        if (temp[i].stage != NULL) {
            printf("create stage %d\n", stage_size);
        } else {
            printf("create stage failed\n");
            exit(-11);
        }
        temp[i].next = atomic_load_explicit(&o_allocator->head, memory_order_relaxed);
        atomic_store_explicit(&o_allocator->head, &temp[i], memory_order_relaxed);
        atomic_init(&temp[i].deref_cnt, 0);
        atomic_init(&temp[i].monitor_sign, 0);
        temp[i].last_deref_cnt = -1;
        
        // init is single thread
    }
    alloc_count += logic_cpu;
    atomic_store_explicit(&o_allocator->current, &temp[logic_cpu - 1], memory_order_release); 

    create_monitor_thread();
    
    return true;        
}

void* monitor_thread(void *arg) {
    // 这里是开线程监测，但是可能会有一个问题，就是该线程一直抢占不到，或者由于缓存一致性倾向于让原本运行的线程继续运行下去
    // 这里先尝试开线程，如果效果不理想那么就尝试用进程
    //监测逻辑
    // 这里访问全局的allcator_out,拿到链表头指针进行遍历
    // 这里我要判断是否应该被reset
    // 这里每隔一段时间遍历一次，使用一个计数器，判断当前值与上一次是否相同
    // 如果相同那么增加计数器，如果两次都相同并且满足使用空间的条件那么执行reset操作
    // 如果值相同且当前计数器值为1，但是空间条件不满足，那么不做任何操作
    // 如果值和上一次不同，那么让计数器值为0,然后继续
    // 则只有第一种值为1并且空间条件满足才会去调用deref_batch,stage_allocator_t的deref_cnt
    // 那么这里的计数器放到哪呢
    // 这里是判断deref_cnt是否有改变所以标志也放在allocator里，并且放在stage中也是意义不明的
    // 然后还需要一个变量用于记录上次的值,以便当前判断和上次是否相同
    // 因为这个操作相当于是管理stage的操作
    stage_allocator_t *head = NULL;
    size_t last_dc = 0;
    
    while(true) {
        printf("monitor_thread begin\n");
        head = atomic_load_explicit(&global_allocator.head, memory_order_acquire);
        while (head) {
            if (atomic_compare_exchange_strong_explicit(&head->deref_cnt, &head->last_deref_cnt, head->last_deref_cnt,
            memory_order_acquire, memory_order_relaxed)) {
                if (head->monitor_sign == 0) {
                    ++head->monitor_sign;
                } else {
                    // 1，判断空间大小
                    size_t cur_used = 0;
                    cur_used = atomic_load_explicit(&head->stage->used, memory_order_acquire);
                    size_t capacity = head->stage->capacity;
                    if (cur_used >= capacity - 256) {
                        printf("stage_deref_batch ");
                        bool r = stage_deref_batch(head->stage, head->last_deref_cnt);
                        if (r) {
                            atomic_fetch_sub_explicit(&head->deref_cnt, Threshold_Deref, memory_order_release);
                        }
                        --head->monitor_sign;
                    }
                }
            } else {
                head->monitor_sign = 0;
                // head->last_deref_cnt = atomic_store_explicit()
            }
            head = head->next;
        }
        printf("monitor_thread sleep ->  3\n");
        sleep(3);
    }
    return NULL;
}

bool create_monitor_thread(void) {

    pthread_t monitor_thread_id;
    pthread_attr_t attr;
    
    // 初始化线程属性
    if (pthread_attr_init(&attr) != 0) {
        perror("pthread_attr_init failed\n");
        return 0;
    }
    
    // 设置线程为分离状态
    if (pthread_attr_setdetachstate(&attr, PTHREAD_CREATE_DETACHED) != 0) {
        perror("pthread_attr_setdetachstate failed\n");
        pthread_attr_destroy(&attr);
        return 0;
    }
    
    // 创建分离线程
    if (pthread_create(&monitor_thread_id, &attr, monitor_thread, NULL) != 0) {
        perror("pthread_create failed");
        pthread_attr_destroy(&attr);
        return 0;
    }
    
    // 销毁属性对象
    pthread_attr_destroy(&attr);
    
    printf("缓存监测分离线程已启动\n");
    return 1;    
}

// //bool stage_allocator_init(stage_allocator_t *allocator) {
// //    if (allocator == NULL) {
// //        printf("allocator not exist\n");
// //        exit(-1);
// //    }
// //    stage_t *temp = NULL;
// //    long logic_cpu = sysconf(_SC_NPROCESSORS_ONLN);
// //    atomic_init(&allocator->head, NULL);
// //    // allocator->head = NULL;
// //    for (int i = 0; i < logic_cpu; i++) {
// //        temp = stage_create(stage_size);
// //        temp->next = atomic_load(&allocator->head);
// //        atomic_store(&allocator->head, temp);
// //    }

//     // atomic_store_explicit(&allocator->stage_total, logic_cpu, memory_order_relaxed);
// //    atomic_store_explicit(&allocator->current, allocator->head, memory_order_release);

// //    return true;
// //}

block_alloc_t allocator_alloc(allocator_out_t *allocator, size_t size) {

    if (allocator == NULL || size == 0) {
        printf("allocator_alloc->(allocator == NULL || size == 0 \
                -> return {0}\n");
        return (block_alloc_t) {
            .ptr = NULL,
            .size = 0,
            .allocator = NULL
        };
    }
    block_alloc_t block = {0};

    stage_allocator_t *stage = atomic_load_explicit(&allocator->current, memory_order_acquire);

    stage_allocator_t  *stage_bak = stage;
    while(stage) {
        
        block = stage_alloc_pessimistic(size, stage);
        if (block.ptr != NULL) {
            if (stage_bak != stage){
                // 这里虽然有current竞争，但是current最终都会指向一个开辟成功的，并且如果有竞争，那么一定是连续的，那么下一次创建，一定会用上之前没用上的stage在最坏情况下
                atomic_store_explicit(&allocator->current, stage, memory_order_release);
            }
            return block;
        }
        stage = stage->next;
    }
    
    // stage_allocator_t *new_allocator = NULL;
//*  这里其实可以加个alloc sign, 上面再加个循环，循环去判断
//* 这样就能保证每次开辟一个，以及下面操作的线程安全
//* 但是CAS重试，也会极大的降低性能
// 这里就先用粗略来
    // 开辟新stage
    printf("create new stage in allocator alloc\n");

    // 这里先使用malloc,后续可以用固定内存池优化
    stage = malloc(sizeof(stage_allocator_t));
    if (stage == NULL) {
        printf("new_lalocator malloc failed\n");
        exit(-10);
    }

    // 这里还需要把stage的current更改到当前新的stage上
    // 但是这里有可能多个任务线程调用alloc然后在旧值current上都不能申请
    // 那么就会多个任务线程同一时刻都去调用alloc
    // 这里还是是否需要精确的问题。这里可以一次性开辟多个
    // 那么这样最差的情况就是current指向最后一个新创建的
    // 如果情况好那么就指向当前的第一个current
    // 这里相当于选择了多创建，但是代价是同一时刻的任务线程都会去创建
    // 如果有某种同步技术，当然还是会有一定的延迟耗时
    // 那么这里就选择粗略创建
    stage->stage = stage_create(stage_size);
    if (stage->stage == NULL) {
        printf("stage is NULL in allocator_alloc\n");
    }
    ++alloc_count;
    block = stage_alloc_pessimistic(size, stage);
    if (block.ptr == NULL) {
        printf("new stage alloc memory failed, error\n");
        exit(-3);
    }


    
    stage_allocator_t *head = atomic_load_explicit(&allocator->head, memory_order_acquire);
    do {
        // 这里把stage_allocator insert to linklist
        stage->next = head;
    } while(!atomic_compare_exchange_weak_explicit(&allocator->head, &head, stage, memory_order_release, memory_order_relaxed));

    // current的赋值，必须在stage被Link到链表后
    // 否则前面的for循环，只会判断当前current的一个，下一个即是NULL
    // 这里还是粗略赋值，最坏情况则是指向最先创建的第一个(先创建的在后面)
    atomic_store_explicit(&allocator->current, stage, memory_order_release);
    return block;
}







// 原子函数， 这里每次调用都是减少1个引用，这里需要设计成能够重用的函数,使用原子操作
// 每次调用内部判断是否达到阈值

// int deref_cnt = 0;
int count_deref = 0;
int deref_arr[100] = {0};
int count_val = 0;
int val_301 = 0;
_Atomic(int) derefcnt = 0;
bool allocator_deref(stage_allocator_t *allocator) {
    // 那么这里肯定就要判断当前值+1是否是阈值
    
    if (allocator == NULL) {
        printf("allocator is NULL\n");
        exit(-3);

    }
    // int cnt_temp = atomic_fetch_add_explicit(&derefcnt, 1, memory_order_release) + 1;
    // printf("cnt_temp -> %d\n", cnt_temp);
    
    
    
    //这里类型使用deref相同的类型,避免溢出导致的逻辑错误,比如uint8_t，500重置，那么永远都到不了500
    size_t temp = 0;
    // temp = atomic_fetch_add_explicit(&allocator->deref_cnt, 1, memory_order_acquire);

    // 这里size_t溢出几乎不可能,那么直接+1拿到当前引用值
    // 如果不+1,那么会在Threshold_Deref + 1才会deref
    // 此时deref后，一直都有1个引用,那么实际上一直都不会reset


    temp = atomic_fetch_add_explicit(&allocator->deref_cnt, 1, memory_order_acquire) + 1;
    // count_val++;
    // if (count_val == 301) {
    //     val_301 = temp;
    // }
    // 这里如果只是判等那么只会调用一次减值
    // 如果其他线程原子add的过快，就算这里触发了一次减少，但是减少后仍然总值大于阈值，那么将永远不会再次调用deref
    // 这里判断是否是Threshold_Deref的倍数
    // 当50时触发，就算其他线程递增的很快，如果我还没递减，那么递增到100会继续触发批量deref操作
    // 如果在两个倍数之间触发批量deref，还没到下一个倍数，那么下面会继续递增到当前倍数然后触发deref
    // if (temp >= Threshold_Deref - 1) {
        // 这里可能会累计很多reference_count
        // 可能deref调用的速度跟不上alloc增加引用的速度
        // 但是取模能保证最终一致性
    // if (temp == 0) return true;
    if (temp % Threshold_Deref == 0) {

        
        
        // size_t ref_cnt = atomic_load_explicit(&allocator->stage->reference_count, memory_order_acquire);
        

        // printf("alloc ref:%ld\n", ref_cnt);
        printf("============ ===================     ============\n");
        printf("stage cnt :%d\n", alloc_count);
        
        // deref_arr[count_deref] = count_val;
        // count_deref ++;
        // 由于是原子操作，所以只有一个线程会获得到当前达到原子,temp也不会两次获得相同的值
        // 调用stage_deref
        // 只有一个线程进入，所以这里是精确计数
        bool r = stage_deref_batch(allocator->stage, Threshold_Deref);
        // test code
        // deref_cnt += Threshold_Deref;     
        // atomic_fetch_add_explicit(&deref_cnt, Threshold_Deref, memory_order_relaxed);
        if (r) {
            atomic_fetch_sub_explicit(&allocator->deref_cnt, Threshold_Deref, memory_order_release);
        }
    }

    // 这里有可能不满足Threshold_Deref的倍数，所以一直不能全部deref,那么引用一直存在，
    // 那么alloc连续失败也不会reset，因为当前存在引用
    // 所以这里需要使用某种策略来
    // 这里可以用一个检测线程，来遍历stage_allocator linklist
    // 上面只是粗略的进行deref,最后的收尾由检测线程来执行
    
    return true;
    
}

//  // block_alloc_t allocator_alloc(stage_allocator_t *allocator, size_t size) {
  
//  //     if (allocator == NULL || size == 0) {
//  //         return (block_alloc_t) {
//  //             .ptr = NULL,
//  //             .size = 0,
//  //             .stage = NULL
//  //         };
//  //     }
  
//  //     block_alloc_t block = {0};
//  //     stage_t *stage = atomic_load_explicit(&allocator->current, memory_order_acquire);
//  //     stage_t *stage_bak = stage;
//  //     while(stage) {
  
//  //         block = stage_alloc_optimistic(size, stage);
//  //         if (block.ptr != NULL) {
//  //             if (stage_bak != stage){
//  //                 atomic_store_explicit(&allocator->current, stage, memory_order_release);
//  //             }
//  //             return block;
//  //         }
//  //         stage = stage->next;
//  //     }
  
  
//  //     // 开辟新stage
//  //     printf("create new stage in allocator alloc\n");
//  
//  //     stage_t *new_stage = stage_create(stage_size);
  
//  //     block = stage_alloc_optimistic(size, stage);
//  //     if (block.ptr == NULL) {
//  //         printf("new stage alloc memory failed, error\n");
//  //         exit(-3);
//  //     }
  
      
//  //     stage_t *head = atomic_load_explicit(&allocator->head, memory_order_acquire);
//  //     do {
//  //         new_stage->next = head;
//  //     } while(!atomic_compare_exchange_weak_explicit(&allocator->head, &head, new_stage, memory_order_release, memory_order_relaxed));
  
//  //     return block;
//  // }