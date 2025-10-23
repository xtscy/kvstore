# include "../stage_allocator.h"
#include <sys/time.h>

extern allocator_out_t global_allocator;
extern  int alloc_count;
typedef struct value_s {

    int a;
    int b;
    
} value_t;



void test1() {
    
    // printf("test1 :-> \n");
    
    if (!allocator_out_init(&global_allocator)) {
        printf("global allocator init failed\n");
        exit(-5);
    }
    
    printf("test1->\n");
    printf("---------------    int    ------------------ \n");
    // printf("1\n");
    
    // 基本数据存储测试
    // int
    int a = -10302121;
    block_alloc_t block = {0};
    printf("size :%lu\n", sizeof(a));
    block = allocator_alloc(&global_allocator, sizeof(a));
    memcpy(block.ptr, &a, sizeof(int));
    printf("block value:%d\n", *((int*)block.ptr));


    printf("---------------   char    ------------------\n");
    // char
    block_alloc_t block_ptr_char = {0};

    char c[25] = {0};
    snprintf(c, sizeof(c), "Hello Allocator\n");
    printf("c : %s\n", c);
     
    printf("strlen(c): %lu", strlen(c));
    block_ptr_char = allocator_alloc(&global_allocator, strlen(c));
    
    printf("block size: %lu\n", block_ptr_char.size);
    memcpy(block_ptr_char.ptr, c, block_ptr_char.size);

    // for (int i = 0; i < block_ptr_char.size; i++) {
    //     printf("current char %d: %c\n", i, *((char*)block_ptr_char.ptr + i));
    // }
    char result[20] = {0};
    memcpy(result, block_ptr_char.ptr, block_ptr_char.size);
    // for (int i = 0; i < block_ptr_char.size; i++) {
    //     printf("current char %d: %c\n", i, *(result + i));
    // }
    printf("result: %s\n", result);

    
    printf("---------------   struct   ------------------\n");
    value_t vt = {
        .a = -10,
        .b = 20000000
    };
    block_alloc_t block_ptr_struct = {0};
    block_ptr_struct = allocator_alloc(&global_allocator, sizeof(value_t));
    memcpy(block_ptr_struct.ptr, &vt, block_ptr_struct.size);
    printf("a: %d, b: %d\n", ((value_t*)block_ptr_struct.ptr)->a, ((value_t*)block_ptr_struct.ptr)->b);
    printf("---------------  pointer   ------------------\n");
    block_alloc_t block_ptr_structPtr = allocator_alloc(&global_allocator, sizeof(value_t*));
    value_t vt2 = {
        .b = 1000000,
        .a = -1000000000
    };
    value_t* value_ptr = &vt2;
    
    memcpy(block_ptr_struct.ptr, &value_ptr, block_ptr_struct.size);
    printf("ptr a: %d, ptr b: %d\n", (*((value_t**)block_ptr_struct.ptr))->a, (*((value_t**)block_ptr_struct.ptr))->b);
}

// typedef struct thread_s {
//     stage_t *stage;
//     int i;
// } thread_t;

#define block_count 1000


void test2() {
    struct timeval start, end;
    long seconds, useconds;
    double duration;
    long total_seconds, total_useconds;
    double total_duration = 0;
    double per_seconds[4] = {0};
    for (int i = 0; i < 4; i++) {
        
    if (!allocator_out_init(&global_allocator)) {
        printf("global allocator init failed\n");
        exit(-5);
    }
    

        block_alloc_t block_temp = {0};
        int count = 0;
        
        printf("first alloc\n");
        gettimeofday(&start, NULL);
        while((block_temp = allocator_alloc(&global_allocator, sizeof count)).size != 0) {
            count++;
            memcpy(block_temp.ptr, &count, block_temp.size);
            printf("count: %d\n", *(int*)block_temp.ptr);
            if (count == block_count) break;
        }
        gettimeofday(&end, NULL);
        seconds  = end.tv_sec  - start.tv_sec;
        useconds = end.tv_usec - start.tv_usec;
        duration = seconds + useconds/1000000.0;
        total_seconds += seconds;
        total_useconds += useconds;
        total_duration += duration;
        per_seconds[i] = seconds;
        // printf("耗时: %f 秒\n", duration);
        // printf("耗时: %ld 微秒\n", seconds * 1000000 + useconds);
        // atomic_store_explicit(global_allocator.head, (stage_allocator_t*)NULL, memory_order_relaxed);
        // atomic_store_explicit(global_allocator.current, (stage_allocator_t*)NULL, memory_order_release);
    }
    total_duration = total_duration / 4;
    double avg_us = (total_seconds * 1000000 + useconds) / 4.0;
    printf("平均耗时:%lf\n", total_duration);
    printf("平均耗时:%lf\n", avg_us);


    for (int i = 0; i < 4; i++) {
        printf("第%d次耗时: %lf\n", i, per_seconds[i]);
        printf("每个块耗时:%lf\n", per_seconds[i] / (double)(block_count));
    }
    
    
    printf("\n");

    printf("开辟stage个数: %d\n", alloc_count);
    
    // printf("alloc failed\n");

// * 50000
//! 平均耗时:1.865042
//! 平均耗时:1955221.750000
//! 第0次耗时: 2.000000
//! 每个块耗时:0.000039
//! 第1次耗时: 2.000000
//! 每个块耗时:0.000039
//! 第2次耗时: 2.000000
//! 每个块耗时:0.000039
//! 第3次耗时: 2.000000
//! 每个块耗时:0.000039



//!     平均耗时:1.791605
//! 平均耗时:1931333.500000
//! 第0次耗时: 2.000000
//! 第1次耗时: 2.000000
//! 第2次耗时: 2.000000
//! 第3次耗时: 1.000000


    // * 100000
//!     平均耗时:5.114983
//! 平均耗时:5212370.750000
//! 第0次耗时: 6.000000
//! 第1次耗时: 5.000000
//! 第2次耗时: 5.000000
//! 第3次耗时: 5.000000

    // * 10240
//!     平均耗时:5.003056
//! 平均耗时:5465896.750000
//! 第0次耗时: 5.000000
//! 第1次耗时: 5.000000
//! 第2次耗时: 5.000000
//! 第3次耗时: 5.000000

    // *500000
//!     平均耗时:25.334946
//! 平均耗时:25764270.250000
//! 第0次耗时: 25.000000
//! 第1次耗时: 24.000000
//! 第2次耗时: 26.000000
//! 第3次耗时: 26.000000
    //* 512000
//!    平均耗时:21.631995
//!    平均耗时:21607907.250000
//!    第0次耗时: 23.000000
//!    每个块耗时:0.000045

//!    第1次耗时: 24.000000
//!    每个块耗时:0.000047

//!    第2次耗时: 21.000000
//!    每个块耗时:0.000041

//!    第3次耗时: 18.000000
//!    每个块耗时:0.000035

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


// mutil thread test
// 上面运行了单线程的alloc性能测试
// 这里测试单线程的deref测试
extern _Atomic(int) deref_cnt;
extern int count_deref;
extern int deref_arr[100];
extern int val_301;
extern int alloc_count;
void test3() {
    if (!allocator_out_init(&global_allocator)) {
        printf("exit init\n");
        exit(-11);
    }
    //这里执行完一个deref一个
    int count = 0;
    block_alloc_t block_temp = {0};
    while((block_temp = allocator_alloc(&global_allocator, sizeof count)).size != 0) {
        count++;
        memcpy(block_temp.ptr, &count, block_temp.size);
        printf("count: %d\n", *(int*)block_temp.ptr);
        allocator_deref(block_temp.allocator);
        if (count == block_count) break;
    }

    int r_deref_cnt = atomic_load_explicit(&deref_cnt, memory_order_acquire);
    printf("deref_cnt : %d\n", r_deref_cnt);
    printf("count: %d\n", count_deref);
    printf("block_count:%d\n", block_count);
    for (int i = 0; i < count_deref; i++) {
        printf("arr[%d]: %d\n", i, deref_arr[i]);
    }

    printf("val 301: %d\n", val_301);
    printf("stage count:%d\n", alloc_count);
    // 那这里需要检验deref的值吗，即deref了多少个，查看误差大概是多少
    // alloc了多少个，然后实际deref了多少个
    // 这里301没有去到deref是开辟了新的stage，并不会一直积累到第一个stage
}








//                                                                                                              //
// mutil thread
//                                                                                                              //


#include <pthread.h>
#include <unistd.h>
#include <stdlib.h>



#define alloc_array_size 800000
block_alloc_t* array = NULL;

void init() {

    array = malloc(sizeof(block_alloc_t) * alloc_array_size);
    memset(array, 0, sizeof(block_alloc_t) * alloc_array_size);
    if (!allocator_out_init(&global_allocator)) {
        printf("init allocator failed\n");
        exit(-10);
    }
}

atomic_int current_write_pos = 0;
atomic_int current_store_cnt = 0;
atomic_int current_read_pos = 0;



pthread_mutex_t mutex = PTHREAD_MUTEX_INITIALIZER;
pthread_cond_t cond = PTHREAD_COND_INITIALIZER;



void *worker_a(void *arg) {

    block_alloc_t block_temp = {0};
    int write_pos = 0;
    for (int i = 0; i <= 1000; i++) {

        block_temp = allocator_alloc(&global_allocator, sizeof(int));
        memcpy(block_temp.ptr, &i, block_temp.size);
        //放到shared array
        // 这里可能add了current_store_cnt后，还没有执行array的赋值操作，使得访问的值为0
        array[write_pos] = block_temp;
        atomic_fetch_add_explicit(&current_store_cnt, 1, memory_order_release);
        write_pos = atomic_fetch_add_explicit(&current_write_pos, 1, memory_order_release) + 1;
        pthread_cond_signal(&cond);
    }
    
    return NULL;
    
}



void *worker_b(void *arg) {
    int current_store = 0;
    int current_read = 0;
// 这里使用锁和条件变量来读取值
// 这里判断
    block_alloc_t temp = {0};
    
    while(true) {
        pthread_mutex_lock(&mutex);
        current_store = atomic_load_explicit(&current_store_cnt, memory_order_acquire);
        while (current_store <= 0) {
            // condition sleep
            pthread_cond_wait(&cond, &mutex);
            current_store = atomic_load_explicit(&current_store_cnt, memory_order_acquire);
        }

        current_read = atomic_fetch_add_explicit(&current_read_pos, 1, memory_order_acquire);
        // 处理数据
        temp = array[current_read];
        printf("process data: %d\n", *(int*)temp.ptr);
        atomic_fetch_sub_explicit(&current_store_cnt, 1, memory_order_relaxed);
        allocator_deref(temp.allocator);
        pthread_mutex_unlock(&mutex);    
    }
    
    
    return NULL;
}

void test4() {
    init();
    pthread_t consume_thread1;
    pthread_t consume_thread2;
    pthread_t product_thread1;
    pthread_t product_thread2;

    pthread_create(&consume_thread1, NULL, worker_b, NULL);
    pthread_create(&consume_thread2, NULL, worker_b, NULL);

    pthread_create(&product_thread1, NULL, worker_a, NULL);

    pthread_join(product_thread1, NULL);
    pthread_join(consume_thread1, NULL);
    pthread_join(consume_thread2, NULL);
    
}





int main() {

    // test1();
    // test2();
    // test3();
    // test4();


    // char buffer[256];
    // printf(buffer);

    
    return 0;
}