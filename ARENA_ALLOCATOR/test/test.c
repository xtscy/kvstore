#include "../arena_allocator.h"
#include <stdlib.h>

typedef struct value_s {
    int a;
    int b;
} value_t;






int main() {

    // 基本数据存储测试
    // int
    printf("---------------    int    ------------------\n");
    block_alloc_t *block_ptr_int = NULL;
    stage_t *stage_ptr = stage_create(STAGE_DATA_SIZE);
    block_ptr_int = stage_alloc(4, stage_ptr);
    int a = -10302121;
    printf("size :%lu\n", sizeof(a));
    memcpy(block_ptr_int->ptr, &a, sizeof(int));
    printf("block value:%d\n", *((int*)block_ptr_int->ptr));

    printf("---------------   char    ------------------\n");
    // char
    char c[15] = {0};
    snprintf(c, sizeof(c), "HELLO WORLD");
    printf("c : %s\n", c);
    
    block_alloc_t *block_ptr_char = NULL;
    printf("strlen(c): %lu", strlen(c));
    block_ptr_char = stage_alloc(strlen(c), stage_ptr);
    printf("block size: %lu\n", block_ptr_char->size);
    memcpy(block_ptr_char->ptr, c, block_ptr_char->size);

    for (int i = 0; i < block_ptr_char->size; i++) {
        printf("current char %d: %c\n", i, *((char*)block_ptr_char->ptr + i));
    }
    char result[20] = {0};
    memcpy(result, block_ptr_char->ptr, block_ptr_char->size);
    for (int i = 0; i < block_ptr_char->size; i++) {
        printf("current char %d: %c\n", i, *(result + i));
    }
    printf("result: %s\n", result);

    // struct ptr or struct 
    value_t vt = {
        .a = 10,
        .b = 20
    };
    printf("---------------   struct   ------------------\n");
    block_alloc_t *block_ptr_struct = stage_alloc(sizeof(value_t), stage_ptr);
    memcpy(block_ptr_struct->ptr, &vt, block_ptr_struct->size);
    printf("a: %d, b: %d\n", ((value_t*)block_ptr_struct->ptr)->a, ((value_t*)block_ptr_struct->ptr)->b);


    printf("---------------  pointer   ------------------\n");

    block_alloc_t *block_ptr_structPtr = stage_alloc(sizeof(value_t*), stage_ptr);
    value_t vt2 = {
        .b = 100,
        .a = -100
    };
    value_t* value_ptr = &vt2;
    
    memcpy(block_ptr_struct->ptr, &value_ptr, block_ptr_struct->size);
    printf("ptr a: %d, ptr b: %d\n", (*((value_t**)block_ptr_struct->ptr))->a, (*((value_t**)block_ptr_struct->ptr))->b);

    // 基本存储测试success

    // 测试减少引用从而自动reset

    

    
    
    return 0;
}