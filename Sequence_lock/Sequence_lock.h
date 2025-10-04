#include <stdio.h>
#include <stdatomic.h>



// 偶数读，奇数写
// 当写操作不复杂时使用序列锁，当写操作很复杂时使用世代锁(降低读的开销)
typedef struct sequence_lock_s {

    atomic_size_t seq_lock;
    
} sequence_lock_t;