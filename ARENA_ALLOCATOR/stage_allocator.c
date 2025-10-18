

#include "stage_allocator.h"



#define Threshold_Deref   100


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
        temp[i].next = atomic_load_explicit(&o_allocator->head, memory_order_relaxed);
        atomic_store_explicit(&o_allocator->head, &temp[i], memory_order_relaxed);
        // init is single thread
    }
    alloc_count += logic_cpu;
    atomic_store_explicit(&o_allocator->current, &temp[logic_cpu - 1], memory_order_release); 
    return true;        
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

int deref_cnt = 0;
int count_deref = 0;
int deref_arr[100] = {0};
int count_val = 0;
int val_301 = 0;
bool allocator_deref(stage_allocator_t *allocator) {
    // 那么这里肯定就要判断当前值+1是否是阈值
    
    if (allocator == NULL) {
        printf("allocator is NULL\n");
        exit(-3);

    }
    
    uint8_t temp = 0;
    
    
    temp = atomic_fetch_add_explicit(&allocator->deref_cnt, 1, memory_order_acquire);
    count_val++;
    if (count_val == 301) {
        val_301 = temp;
    }
    // 这里如果只是判等那么只会调用一次减值
    // 如果其他线程原子add的过快，就算这里触发了一次减少，但是减少后仍然总值大于阈值，那么将永远不会再次调用deref
    // 这里判断是否是Threshold_Deref的倍数
    // 当50时触发，就算其他线程递增的很快，如果我还没递减，那么递增到100会继续触发批量deref操作
    // 如果在两个倍数之间触发批量deref，还没到下一个倍数，那么下面会继续递增到当前倍数然后触发deref
    // if (temp >= Threshold_Deref - 1) {
    if (temp == 0) return true;
    // 这里可能会累计很多reference_count
    // 可能deref调用的速度跟不上alloc增加引用的速度
    // 但是取模能保证最终一致性
    if (temp % Threshold_Deref == 0) {

        
        
        size_t ref_cnt = atomic_load_explicit(&allocator->stage->reference_count, memory_order_acquire);
        printf("alloc ref:%ld\n", ref_cnt);
        printf("stage cnt :%d\n", alloc_count);
        deref_arr[count_deref] = count_val;
        count_deref ++;
        // 由于是原子操作，所以只有一个线程会获得到当前达到原子,temp也不会两次获得相同的值
        // 调用stage_deref
        // 只有一个线程进入，所以这里是精确计数
        stage_deref_batch(allocator->stage, Threshold_Deref);
        // test code
        deref_cnt += Threshold_Deref;     
        // atomic_fetch_add_explicit(&deref_cnt, Threshold_Deref, memory_order_relaxed);
        atomic_fetch_sub_explicit(&allocator->deref_cnt, Threshold_Deref, memory_order_release);
    }

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