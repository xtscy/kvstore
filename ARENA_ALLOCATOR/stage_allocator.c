

#include "stage_allocator.h"



#define Threshold_Deref   100




stage_allocator_t global_stage_allocator = {0};



bool allocator_out_init(allocator_out_t *o_allocator) {

    if (o_allocator == NULL) {
        printf("init out allocator falied\n");
        exit(-1);
    }
    
    long logic_cpu = sysconf(_SC_NPROCESSORS_ONLN);
    atomic_init_explicit(&o_allocator->head, NULL, memory_order_relaxed);
    atomic_init_explicit(&o_allocator->current, NULL, memory_order_relaxed);
    stage_allocator_t *temp = NULL;
    temp = malloc(sizeof(sizeof(stage_allocator_t) * logic_cpu));
    for (int i = 0; i < logic_cpu; i++) {
        temp[i].stage = stage_create(stage_size);
        temp[i].next = atomic_load_explicit(&o_allocator->head, memory_order_relaxed);
        atomic_store_explicit(&o_allocator->head, &temp[i], memory_order_relaxed);
        // init is single thread
    }

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
            .stage = NULL
        };
    }
    block_alloc_t block = {0};
    stage_t *stage = atomic_load_explicit(&allocator->current, memory_order_acquire);
    stage_t *stage_bak = stage;
    while(stage) {
        
        block = stage_alloc_pessimistic(size, stage);
        if (block.ptr != NULL) {
            if (stage_bak != stage){
                atomic_store_explicit(&allocator->current, stage, memory_order_release);
            }
            return block;
        }
        stage = stage->next;
    }
    
    stage_allocator_t *new_allocator = NULL;

    // 开辟新stage
    printf("create new stage in allocator alloc\n");

    // 这里先使用malloc,后续可以用固定内存池优化
    new_allocator = malloc(sizeof(stage_allocator_t));
    new_allocator->stage = stage_create(stage_size);
    block = stage_alloc_pessimistic(size, stage);
    if (block.ptr == NULL) {
        printf("new stage alloc memory failed, error\n");
        exit(-3);
    }


    
    stage_t *head = atomic_load_explicit(&allocator->head, memory_order_acquire);
    do {
        // 这里把stage_allocator insert to linklist
        new_allocator->next = head;
    } while(!atomic_compare_exchange_weak_explicit(&allocator->head, &head, new_allocator, memory_order_release, memory_order_relaxed));

    return block;
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



// 原子函数， 这里每次调用都是减少1个引用，这里需要设计成能够重用的函数,使用原子操作
// 每次调用内部判断是否达到阈值

bool allocator_deref(stage_allocator_t *allocator) {
    // 那么这里肯定就要判断当前值+1是否是阈值
    
    if (allocator == NULL) {
        printf("allocator is NULL\n");
        exit(-3);

    }
    
    uint8_t temp = 0;
    
    
    temp = atomic_fetch_add_explicit(&allocator->deref_cnt, 1, memory_order_acquire);
    // 这里如果只是判等那么只会调用一次减值
    // 如果其他线程原子add的过快，就算这里触发了一次减少，但是减少后仍然总值大于阈值，那么将永远不会再次调用deref
    // 这里判断是否是Threshold_Deref的倍数
    // 当50时触发，就算其他线程递增的很快，如果我还没递减，那么递增到100会继续触发批量deref操作
    // 如果在两个倍数之间触发批量deref，还没到下一个倍数，那么下面会继续递增到当前倍数然后触发deref
    // if (temp >= Threshold_Deref - 1) {
    if (temp % Threshold_Deref == 0) {
        // 由于是原子操作，所以只有一个线程会获得到当前达到原子,temp也不会两次获得相同的值
        // 调用stage_deref
        // 只有一个线程进入，所以这里是精确计数
        stage_deref_batch(allocator->stage, Threshold_Deref);
        atomic_fetch_sub_explicit(&allocator->deref_cnt, Threshold_Deref, memory_order_release);
    }

    return true;
    
}

