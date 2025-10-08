#include <unistd.h>
#include "../arena_allocator.h"
#include <stdlib.h>
#include <pthread.h>
#include <sys/time.h>
#include <stdatomic.h>
typedef struct value_s {
    int a;
    int b;
} value_t;



void test2() {
    /*
    printf("test2()\n");
    stage_t *stage_ptr = stage_create(STAGE_DATA_SIZE);
    // 基本数据存储测试
    // int
    printf("---------------    int    ------------------\n");
    block_alloc_t block_ptr_int = {0};
    block_ptr_int = stage_alloc(4, stage_ptr);
    int a = -10302121;
    memcpy(block_ptr_int.ptr, &a, sizeof(int));
    printf("block value:%d\n", *((int*)block_ptr_int.ptr));

    printf("---------------   char    ------------------\n");
    // char
    char c[15] = {0};
    snprintf(c, sizeof(c), "HELLO WORLD");
    printf("c : %s\n", c);
     
    block_alloc_t block_ptr_char = {0};
    printf("strlen(c): %lu", strlen(c));
    block_ptr_char = stage_alloc(strlen(c), stage_ptr);
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

    // struct ptr or struct 
    value_t vt = {
        .a = 10,
        .b = 20
    };
    printf("---------------   struct   ------------------\n");
    block_alloc_t block_ptr_struct = stage_alloc(sizeof(value_t), stage_ptr);
    memcpy(block_ptr_struct.ptr, &vt, block_ptr_struct.size);
    printf("a: %d, b: %d\n", ((value_t*)block_ptr_struct.ptr)->a, ((value_t*)block_ptr_struct.ptr)->b);

    printf("---------------  pointer   ------------------\n");

    block_alloc_t block_ptr_structPtr = stage_alloc(sizeof(value_t*), stage_ptr);
    value_t vt2 = {
        .b = 100,
        .a = -100
    };
    value_t* value_ptr = &vt2;
    
    memcpy(block_ptr_struct.ptr, &value_ptr, block_ptr_struct.size);
    printf("ptr a: %d, ptr b: %d\n", (*((value_t**)block_ptr_struct.ptr))->a, (*((value_t**)block_ptr_struct.ptr))->b);
    */
    
}

void test3() {

    stage_t *stage = NULL;
    stage = stage_create(128);
    block_alloc_t block_temp = {0};
    int count = 0;

    printf("first alloc\n");
    while((block_temp = stage_alloc(sizeof count, stage)).size != 0) {
        count++;
        memcpy(block_temp.ptr, &count, block_temp.size);
        printf("count: %d\n", count);
    }
    
    printf("alloc failed\n");
    
    while(count > 0) {
        stage_deref(stage);
        count--; 
    }
    
    printf("second alloc\n");
    while((block_temp = stage_alloc(sizeof count, stage)).size != 0) {
        count++; 
        memcpy(block_temp.ptr, &count, block_temp.size);
        printf("count: %d\n", count);
    }
    
}

// 多线程测试
// 2个线程同时向stage申请空间，保证每次申请后访问的正确性


#define END_VALUE 20000
atomic_int cnt;

typedef struct thread_s {
    stage_t *stage;
    int i;
} thread_t;

void *worker(void *arg) {

    thread_t *tt = (thread_t*)arg;
    stage_t *stage = tt->stage;
    int id = tt->i;

    // char c[30] = {0};
    
    int *memory = malloc(sizeof(int*) * 99999);
    int memory_cnt = 0;
    // stage_t *stage = (stage_t*)arg;
    int count = 0;
    block_alloc_t block_temp = {0};
    int current = 0;
    // if (id == 0) {
    //     return NULL;
    // }
    while (true) {
        // snprintf(c, sizeof(c), "thread %d: %d", id, count);
        block_temp = stage_alloc(sizeof(int), stage);
        if (block_temp.size != 0) {
            count++;
            // printf("%s\n", block_temp.ptr);

            memory[memory_cnt] = memory_cnt + 1;
            memory_cnt++;
        } else {
            // printf("sleep 1\n");
            // sleep(1);
            if (count != 0) {
                for (int i = current; i < memory_cnt; i++, current++) {
                    printf("memory %d: %d\n", id, memory[i]);
                    if (memory[i] == END_VALUE) {
                        // printf("sleep 1\n");
                        // sleep(1);
                        while (count > 0) {
                            stage_deref(stage);
                            count --;
                        }            
                        return NULL;
                    }
                }
            }
            while (count > 0) {
                stage_deref(stage);
                count --;
            }

            if (id == 1) {
                usleep(7);
            }
            
            // printf("sleep 2\n");
            // sleep(2);
            
            // printf("sleep 1 outer\n");
            // sleep(1);
        }
    }
}


void test4() {
    stage_t *stage = stage_create(128);
    pthread_t id[2] = {0};
    thread_t tt1 = {
        .stage = stage,
        .i = 0
    };
    thread_t tt2 = {
        .stage = stage,
        .i = 1
    };
    struct timeval start, end;
    long seconds, useconds;
    double duration;
    long total_seconds, total_useconds;
    double total_duration;
    
    for (int i = 0; i < 10; i++) {

        gettimeofday(&start, NULL);    
        pthread_create(&(id[0]), NULL, worker, &tt1);
        pthread_create(&(id[1]), NULL, worker, &tt2);

        pthread_join(id[0], NULL);
        pthread_join(id[1], NULL);

        gettimeofday(&end, NULL);
        
        seconds  = end.tv_sec  - start.tv_sec;
        useconds = end.tv_usec - start.tv_usec;
        duration = seconds + useconds/1000000.0;
        total_seconds += seconds;
        total_useconds += useconds;
        total_duration += duration;
        printf("耗时: %f 秒\n", duration);
        printf("耗时: %ld 微秒\n", seconds * 1000000 + useconds);
    }
    total_duration = total_duration / 10;
    double avg_us = (total_seconds * 1000000 + useconds) / 10.0;
    printf("平均耗时:%lf\n", total_duration);
    printf("平均耗时:%lf\n", avg_us);
    
}

void test5() {
    struct timeval start, end;
    long seconds, useconds;
    double duration;
    long total_seconds, total_useconds;
    double total_duration;
    

    for (int i = 0; i < 10; i++) {

        int count = 0;
        gettimeofday(&start, NULL);    
        while (count != END_VALUE * 2) {
            count++;
            printf("count %d\n", count);
        }
        gettimeofday(&end, NULL);
        
        seconds  = end.tv_sec  - start.tv_sec;
        useconds = end.tv_usec - start.tv_usec;
        duration = seconds + useconds/1000000.0;
        total_seconds += seconds;
        total_useconds += useconds;
        total_duration += duration;
        printf("耗时: %f 秒\n", duration);
        printf("耗时: %ld 微秒\n", seconds * 1000000 + useconds);
    }
    total_duration = total_duration / 10;
    double avg_us = (total_seconds * 1000000 + useconds) / 10.0;
    printf("平均耗时:%lf\n", total_duration);
    printf("平均耗时:%lf\n", avg_us);

}
/*
    第一次
    耗时: 2.746557 秒
    耗时: 2746557 微秒

    耗时: 3.840547 秒
    耗时: 3840547 微秒

    耗时: 3.346874 秒
    耗时: 3346874 微秒

    耗时: 3.440316 秒
    耗时: 3440316 微秒

    耗时: 3.402865 秒
    耗时: 3402865 微秒
*/
/*
    耗时: 2.787718 秒
    耗时: 2787718 微秒

    耗时: 2.694656 秒
    耗时: 2694656 微秒

    耗时: 2.747979 秒
    耗时: 2747979 微秒

    耗时: 2.697477 秒
    耗时: 2697477 微秒

    耗时: 2.851465 秒
    耗时: 2851465 微秒
*/

int main() {
    // block_alloc_t *block_ptr_int = NULL;
    // stage_t *stage_ptr = stage_create(STAGE_DATA_SIZE);
/*
    // 基本数据存储测试
    // int
    printf("---------------    int    ------------------\n");
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
*/
    // 测试减少引用从而自动reset

    // 这里先分配，分配完后，进行统一的deref,全部deref完，那么将会自动reset
    //这里如果要deref，那么就是肯定一个alloc一个deref,那么也就是可以直接用一个计数来记录alloc了多少次
    // block_alloc_t *block_alloc_temp = NULL;
    // int count = 0;
    // while(block_alloc_temp = stage_alloc(sizeof(int), stage_ptr)) {
    //     count++;
    //     memcpy(block_alloc_temp->ptr, &count, block_alloc_temp->size);
    //     printf("current count: %d\n", *(int*)block_alloc_temp->ptr);
    // }
    // printf("block_alloc_t size : %lu\n", sizeof(block_alloc_t));
    // printf("alloc failed\n");
    
    // test2();
    // test3();
    // test4();
    /* 多线程无锁性能测试，1个线程20000个数据，跑2个线程
    平均耗时:1.384397
    平均耗时:1929220.600000
    */
    
    // test5();
    /* 单线程跑40000数据，1次读写测试 
    平均耗时:1.469760
    平均耗时:1546512.500000
    */

    // 多线程可能比单线程更快，但是肯定大概率单线程更快
    return 0;
}