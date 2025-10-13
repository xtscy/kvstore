#include "../stage_allocator.h"

extern stage_allocator_t global_stage_allocator;

typedef struct value_s {
    int a;
    int b;
} value_t;

void test1() {
     
    
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


    printf("---------------   char    ------------------\n");
    // char
    block_alloc_t block_ptr_char = {0};

    char c[15] = {0};
    snprintf(c, sizeof(c), "HELLO WORLD");
    printf("c : %s\n", c);
     
    printf("strlen(c): %lu", strlen(c));
    block_ptr_char = allocator_alloc(&global_stage_allocator, strlen(c));
    
    printf("block size: %lu\n", block_ptr_char.size);
    memcpy(block_ptr_char.ptr, c, block_ptr_char.size);

    for (int i = 0; i < block_ptr_char.size; i++) {
        printf("current char %d: %c\n", i, *((char*)block_ptr_char.ptr + i));
    }
    char result[20] = {0};
    memcpy(result, block_ptr_char.ptr, block_ptr_char.size);
    for (int i = 0; i < block_ptr_char.size; i++) {
        printf("current char %d: %c\n", i, *(result + i));
    }
    printf("result: %s\n", result);

    
    printf("---------------   struct   ------------------\n");
    value_t vt = {
        .a = -10,
        .b = 20000000
    };
    block_alloc_t block_ptr_struct = {0};
    block_ptr_struct = allocator_alloc(&global_stage_allocator, sizeof(value_t));
    memcpy(block_ptr_struct.ptr, &vt, block_ptr_struct.size);
    printf("a: %d, b: %d\n", ((value_t*)block_ptr_struct.ptr)->a, ((value_t*)block_ptr_struct.ptr)->b);
    printf("---------------  pointer   ------------------\n");
    block_alloc_t block_ptr_structPtr = allocator_alloc(&global_stage_allocator, sizeof(value_t*));
    value_t vt2 = {
        .b = 1000000,
        .a = -1000000000
    };
    value_t* value_ptr = &vt2;
    
    memcpy(block_ptr_struct.ptr, &value_ptr, block_ptr_struct.size);
    printf("ptr a: %d, ptr b: %d\n", (*((value_t**)block_ptr_struct.ptr))->a, (*((value_t**)block_ptr_struct.ptr))->b);

}


// 测试 一直申请内存
void test2() {
    if (!stage_allocator_init(&global_stage_allocator)) {
        printf("global allocator init failed\n");
        exit(-5);
    }

    block_alloc_t block_temp = {0};
    int count = 0;

    printf("first alloc\n");
    while((block_temp = allocator_alloc(&global_stage_allocator, sizeof count)).size != 0) {
        count++;
        memcpy(block_temp.ptr, &count, block_temp.size);
        printf("count: %d\n", count);
    }
    printf("alloc failed\n");
    


    // while(count > 0) {
    //     stage_deref(stage);
    //     count--; 
    // }
    
    

    
    // printf("second alloc\n");
    // while((block_temp = stage_alloc(sizeof count, stage)).size != 0) {
    //     count++; 
    //     memcpy(block_temp.ptr, &count, block_temp.size);
    //     printf("count: %d\n", count);
    // }
    
}
// muti thread test
void test3() {

    
    
}


int main() {

    // test1();
    test2();
    
    
}