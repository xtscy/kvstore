#define _GNU_SOURCE
#include <pthread.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>

// 或者使用动态初始化
// pthread_rwlock_init(&rwlock, NULL);

// 获取读锁
// int pthread_rwlock_rdlock(pthread_rwlock_t *rwlock);

// 尝试获取读锁（非阻塞）
// int pthread_rwlock_tryrdlock(pthread_rwlock_t *rwlock);

// 获取写锁
// int pthread_rwlock_wrlock(pthread_rwlock_t *rwlock);

// 尝试获取写锁（非阻塞）
// int pthread_rwlock_trywrlock(pthread_rwlock_t *rwlock);

// 释放锁
// int pthread_rwlock_unlock(pthread_rwlock_t *rwlock);

// 销毁锁
// int pthread_rwlock_destroy(pthread_rwlock_t *rwlock);



pthread_rwlock_t rwlock = PTHREAD_RWLOCK_INITIALIZER;
int shared_data = 0;

void* writer(void *arg) {
    while(1) {
        pthread_rwlock_wrlock(&rwlock);
        printf("wirter run\n");
        shared_data++;
        pthread_rwlock_unlock(&rwlock);
        sleep(2);
    }
}

void* reader(void *arg) {
    while(1) {
        pthread_rwlock_wrlock(&rwlock);
        printf("shared data:%d\n", shared_data);
        pthread_rwlock_unlock(&rwlock);
        sleep(1);
    }
}

int main() {

    // 这里让写锁写数据，然后让读者一直去读
    pthread_t wr_thread = 0;

    pthread_create(&wr_thread, NULL, writer, NULL);
    pthread_t parr[5] = {0};
    for (int i = 0; i < 5; i++) {
        pthread_create(&parr[i], NULL, reader, NULL);
    }

    for (int i = 0; i < 5; i++) {
        pthread_join(parr[i],NULL);
    }

    
    return 0;
}