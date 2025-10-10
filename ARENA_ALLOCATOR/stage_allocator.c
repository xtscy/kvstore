

#include "stage_allocator.h"


#define stage_size 1024*512


stage_allocator_t global_stage_allocator = {0};

bool stage_allocator_init(stage_allocator_t *allocator) {
    if (allocator == NULL) {
        printf("allocator not exist\n");
        exit(-1);
    }
    stage_t *temp = NULL;
    long logic_cpu = sysconf(_SC_NPROCESSORS_ONLN);
    atomic_init(&allocator->head, NULL);
    // allocator->head = NULL;
    for (int i = 0; i < logic_cpu; i++) {
        temp = stage_create(stage_size);
        temp->next = atomic_load(&allocator->head);
        atomic_store(&allocator->head, temp);
    }

    // atomic_store_explicit(&allocator->stage_total, logic_cpu, memory_order_relaxed);
    atomic_store_explicit(&allocator->current, allocator->head, memory_order_release);

    return true;
}


block_alloc_t allocator_alloc(stage_allocator_t *allocator, size_t size) {

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

        block = stage_alloc(size, stage);
        if (block.ptr != NULL) {
            if (stage_bak != stage){
                atomic_store_explicit(&allocator->current, stage, memory_order_release);
            }
            return block;
        }
        stage = stage->next;
    }


    // 开辟新stage
    printf("create new stage in allocator alloc\n");

    stage_t *new_stage = stage_create(stage_size);

    block = stage_alloc(size, stage);
    if (block.ptr == NULL) {
        printf("new stage alloc memory failed, error\n");
        exit(-3);
    }

    
    stage_t *head = atomic_load_explicit(&allocator->head, memory_order_acquire);
    do {
        new_stage->next = head;
    } while(!atomic_compare_exchange_weak_explicit(&allocator->head, &head, new_stage, memory_order_release, memory_order_relaxed));

    return block;
}



