
#include <stdatomic.h>
#include <stdio.h>
#include <unistd.h>
#include <pthread.h>

int data = 0;
_Atomic int ready = 0;//* 原子标志

void* func1(void *arg) { //* 生产者
    // printf("sleep 5s");//* 模拟数据准备过程
    sleep(1);
    data = 1000;
    //* release保证在这条语句的程序顺序之前的存储操作，都全部发布
    atomic_store_explicit(&ready, 1, memory_order_release);

    printf("func1 数据已发布\n");
    return NULL;
}



void* func2(void *arg) { //* 消费者
    //   while (atomic_load_explicit(&shared.ready, memory_order_acquire) == 0) {
    //     // 可以在这里做一些其他工作
    //     usleep(100000);  // 休眠100ms避免忙等待
    // }
    int observed = 0;
    //* load不保证读到原子操作的值，只是保证如果读到store的值，那么就有一致性的保证
    do {
        observed = atomic_load_explicit(&ready, memory_order_acquire);
    // 可能多次看到0，但一旦看到1，就能保证一致性
    } while (observed != 1);  // 循环直到看到期望的值
    // atomic_load_explicit(&ready, memory_order_acquire);
    printf("func2: data->%d\n", data);
}


int main() {
    pthread_t fun1, fun2;
    pthread_create(&fun2, NULL, func2, NULL);
    pthread_create(&fun1, NULL, func1, NULL);

    pthread_join(fun2, NULL);//* 先等待消费者，以此体现出内存屏障的作用，因为，消费者内部使用了acquire.
    pthread_join(fun1, NULL);
    
    return 0;
}