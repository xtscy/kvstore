#include "../stage_allocator.h"

extern stage_allocator_t global_stage_allocator;

int main() {
    
    
    if (!stage_allocator_init(&global_stage_allocator)) {
        printf("global allocator init failed\n");
        exit(-5);
    }
    
    
    block_alloc_t block = {0};
    // 基本数据存储测试
    // int
    printf("---------------    int    ------------------\n");
    int a = -10302121;
    printf("size :%lu\n", sizeof(a));
    block = allocator_alloc(&global_stage_allocator, sizeof(a));
    memcpy(block.ptr, &a, sizeof(int));
    printf("block value:%d\n", *((int*)block.ptr));
    
    return 0;
}