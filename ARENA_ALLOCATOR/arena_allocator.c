#include "arena_allocator.h"

// size_t block_size = sizeof(block_alloc_t);
// 这里使用了sequence lock,读和写都需要lock涉及，才能达到正常效果
stage_t *stage_create(size_t capacity)
{
    stage_t *stage = NULL;
    // create allocator
    // size_t size = sizeof(stage_t) + capacity;
    size_t size = capacity;
    if ((stage = malloc(size)) != NULL)
    {
        stage->capacity = capacity - sizeof(stage_t);
        // 这里在create里可以不用锁，因为当前stage还未链接到链表中，并不会被多个线程访问，最多就是返回的当前线程
        atomic_init(&stage->used, 0);
        atomic_init(&stage->reference_count, 0);
        atomic_init(&stage->fail_cnt, 0);
        // 这里只是初始化next为NULL,由外部来做链接操作
        stage->next = NULL;
        atomic_init(&stage->lock.seq_lock, 0);
        return stage;
    }
    printf("create stage allocator failed!\n");
    exit(-5);
    // return NULL;
}

// sequence lock
//  序列锁是读循环，不是写循环
//  当需要更新多个原子变量，并且保证同步性时。
//  可以使用序列锁， 在写锁内部使用原子操作更改值
//  因为写锁阻塞了所有的写操作
//  并且这里没有回滚，因为当前已经占有锁
//  只有当操作成功时才会释放序列锁
//  在中间阶段不会有其他写操作打扰，所以不需要回滚

// 批处理直接开辟空间，并且在当前用完后--ref
// 对于cache的分配，每次同样incref,写在cache_alloc中
block_alloc_t stage_alloc_optimistic(size_t size, stage_allocator_t *allocator)
{
    stage_t *stage = allocator->stage;
    // 单线程的控制依赖已经为CPU提供了足够的排序保证
    // 现代CPU会尊重这种依赖
    if (size == 0 || stage == NULL)
    {
        printf("size or/and stage is unfair\n");
        exit(-1);
    }

    // 在参数有效后，开始定义变量
    size_t seq; // 这里可以不写初始值，赋值随机变量
    size_t current_used, new_used;

    do
    {
        // 目前不需要可见性同步语义
        seq = atomic_load_explicit(&stage->lock.seq_lock, memory_order_acquire);
        //! CPU会保证正确的控制流排序执行,以便于不改变控制流逻辑
        if (seq & 1)
        {
            continue;
        }

        // 当前序列锁没有被写锁，这里再读used判断是否还有空位。写的时候才锁，
        current_used = atomic_load_explicit(&stage->used, memory_order_relaxed);
        // 内嵌管理结构
        // new_used = current_used + sizeof(block_alloc_t) + size;
        new_used = current_used + size;
        // 空间不足是alloc fail的一种方法
        // 乐观锁的操作失败，有一定的随机性
        // 这里有可能序列锁更改更本不满足，
        if (new_used >= stage->capacity)
        {
            atomic_fetch_add_explicit(&stage->fail_cnt, 1, memory_order_relaxed);
            printf("当前stage容量不足,无法申请\n");
            return (block_alloc_t){
                .ptr = NULL,
                .size = 0,
                .allocator = NULL};
        }

        // 这里要增加fail_cnt,那么必须在判断容量不足，且seq序列不变得情况下，否则
        // 其他操作对锁已经进行了抢占和更改，那么这里原本可能是可以分配的吗？
        // 可能原本是不能分配的，但是有deref操作触发了reset导致，当前的操作能够进行alloc
        // 那么这样fail_cnt相当于是不精确的计数
        // 那么问题就是，当前需要精确计数吗
        // 精确计数会用更多的原子操作，但是粗略计数，允许一些误差，可能会触发更多的reset尝试
        // 所以这里应该仔细权衡，或者进行实际测试
        // 精确测试，使用悲观锁，这样能够精确fail计数
        // 这里实现两个版本

        // 进行写操作，先获取写锁, 如果true那么获取写锁成功，否则如果为false获取失败
        if (atomic_compare_exchange_weak_explicit(&stage->lock.seq_lock, &seq, seq + 1,
                                                  memory_order_acquire, memory_order_relaxed))
        {
            // 进行写操作,更改new_used
            atomic_store_explicit(&stage->used, new_used, memory_order_relaxed);
            atomic_fetch_add_explicit(&stage->reference_count, 1, memory_order_relaxed);
            // 上面两个relaxed,谁先执行都可以
            // 更新完后释放序列锁 ,这里不用CAS，因为奇数其他写不了这里不用比较原值，所以直接add
            atomic_fetch_add_explicit(&stage->lock.seq_lock, 1, memory_order_release);
            break;
        }
    } while (true);

    // block_alloc_t *ret = (block_alloc_t *)(stage->init_pos + current_used);
    // ret->ptr = stage->init_pos + current_used + sizeof(block_alloc_t);
    // ret->size = size;
    // ret->stage = stage;
    // return (block_alloc_t*)(stage->init_pos + current_used);
    return (block_alloc_t){
        .ptr = stage->init_pos + current_used,
        .size = size,
        .allocator = allocator};
}
//
block_alloc_t stage_alloc_pessimistic(size_t size, stage_allocator_t *allocator)
{
    stage_t *stage = allocator->stage;
    // 单线程的控制依赖已经为CPU提供了足够的排序保证
    // 现代CPU会尊重这种依赖
    if (size == 0 || stage == NULL)
    {
        printf("size or/and stage is unfair\n");
        exit(-1);
    }

    // 在参数有效后，开始定义变量
    size_t seq; // 这里可以不写初始值，赋值随机变量
    size_t current_used, new_used;

    do
    {
        
        seq = atomic_load_explicit(&stage->lock.seq_lock, memory_order_acquire);
        if (seq & 1)
        {
            continue;
        }

        // 当前序列锁没有被写锁，这里再读used判断是否还有空位。写的时候才锁，
        // 内嵌管理结构
        // new_used = current_used + sizeof(block_alloc_t) + size;
        // 空间不足是alloc fail的一种方法
        // 乐观锁的操作失败，有一定的随机性
        // 这里有可能序列锁更改更本不满足，
        // if (new_used >= stage->capacity) {
            //     atomic_fetch_add_explicit(&stage->fail_cnt, 1, memory_order_relaxed);
            //     printf("当前stage容量不足,无法申请\n");
            //     return (block_alloc_t){
                //         .ptr = NULL,
                //         .size = 0,
                //         .stage = NULL
                //     };
                // }
                
                // 这里要增加fail_cnt,那么必须在判断容量不足，且seq序列不变得情况下，否则
        // 其他操作对锁已经进行了抢占和更改，那么这里原本可能是可以分配的吗？
        // 可能原本是不能分配的，但是有deref操作触发了reset导致，当前的操作能够进行alloc
        // 那么这样fail_cnt相当于是不精确的计数
        // 那么问题就是，当前需要精确计数吗
        // 精确计数会用更多的原子操作，但是粗略计数，允许一些误差，可能会触发更多的reset尝试
        // 所以这里应该仔细权衡，或者进行实际测试
        // 精确测试，使用悲观锁，这样能够精确fail计数
        // 这里实现两个版本
        
        // 进行写操作，先获取写锁, 如果true那么获取写锁成功，否则如果为false获取失败
        if (atomic_compare_exchange_weak_explicit(&stage->lock.seq_lock, &seq, seq + 1,
            memory_order_acquire, memory_order_relaxed))
        {
            
            current_used = atomic_load_explicit(&stage->used, memory_order_relaxed);
            new_used = current_used + size;
            if (new_used > stage->capacity)
            {
                // uint8_t temp_fail = atomic_fetch_add_explicit(&stage->fail_cnt, 1, memory_order_relaxed);
                // size_t temp_ref = atomic_load_explicit(&stage->reference_count, memory_order_relaxed);
                // if (temp_fail >= 10 && temp_ref == 0)
                // {
                //     printf("alloc 连续失败，进行reset后alloc\n");
                //     // alloc 失败连续10次 ref为0
                //     // 并且这里不会连续调用该stage所以fail不会一直增大，且ref不为0
                //     // reset即把偏移量置为0，且把当前失败次数置为0
                //     if (size <= stage->capacity){
                //         atomic_store_explicit(&stage->fail_cnt, 0, memory_order_relaxed);
                //         current_used = 0;
                //         new_used = size;
                //     } else {
                //         atomic_store_explicit(&stage->used, 0, memory_order_relaxed);
                //         atomic_store_explicit(&stage->fail_cnt, 0, memory_order_relaxed);
                //         return (block_alloc_t){
                //             .ptr = NULL,
                //             .size = 0,
                //             .allocator = NULL
                //         };

                //     }
                //     // 这里重置后，可以继续使用该stage进行分配
                // } else {
                    atomic_fetch_add_explicit(&stage->lock.seq_lock, 1, memory_order_release);
                    // printf("当前stage容量不足,无法申请\n");
                    // printf("stage_alloc_pessimistic exit");
                    // exit(-11);
                    return (block_alloc_t){
                        .ptr = NULL,
                        .size = 0,
                        .allocator = NULL};
                // }
            }

            // 进行写操作,更改new_used
            atomic_store_explicit(&stage->used, new_used, memory_order_relaxed);
            atomic_fetch_add_explicit(&stage->reference_count, 1, memory_order_relaxed);
            // 上面两个relaxed,谁先执行都可以
            // 更新完后释放序列锁 ,这里不用CAS，因为奇数其他写不了这里不用比较原值，所以直接add
            atomic_fetch_add_explicit(&stage->lock.seq_lock, 1, memory_order_release);
            break;
        }
    } while (true);

    // block_alloc_t *ret = (block_alloc_t *)(stage->init_pos + current_used);
    // ret->ptr = stage->init_pos + current_used + sizeof(block_alloc_t);
    // ret->size = size;
    // ret->stage = stage;
    // return (block_alloc_t*)(stage->init_pos + current_used);
    return (block_alloc_t){
        .ptr = stage->init_pos + current_used,
        .size = size,
        .allocator = allocator};
}

// block_alloc_t *stage_alloc(size_t size, stage_t *stage) {

//     // 单线程的控制依赖已经为CPU提供了足够的排序保证
//     // 现代CPU会尊重这种依赖
//     if (size == 0 || stage == NULL) {
//         printf("size or/and stage is unfair\n");
//         return NULL;
//     }

//     // 在参数有效后，开始定义变量
//     size_t seq;// 这里可以不写初始值，赋值随机变量
//     size_t current_used, new_used;

//     do {
//         // 目前不需要可见性同步语义
//         seq = atomic_load_explicit(&stage->lock.seq_lock, memory_order_acquire);
//         //! CPU会保证正确的控制流排序执行,以便于不改变控制流逻辑
//         if (seq & 1) {
//             continue;
//         }

//         //当前序列锁没有被写锁，这里再读used判断是否还有空位。写的时候才锁，
//         current_used = atomic_load_explicit(&stage->used, memory_order_relaxed);
//         // 内嵌管理结构
//         new_used = current_used + sizeof(block_alloc_t) + size;
//         if (new_used >= stage->capacity) {
//             printf("当前stage容量不足,无法申请");
//             return NULL;
//         }

//         // 进行写操作，先获取写锁, 如果true那么获取写锁成功，否则如果为false获取失败
//         if (atomic_compare_exchange_weak_explicit(&stage->lock.seq_lock, &seq, seq + 1,
//             memory_order_acquire, memory_order_relaxed)) {
//             // 进行写操作,更改new_used
//             atomic_store_explicit(&stage->used, new_used, memory_order_relaxed);
//             atomic_fetch_add_explicit(&stage->reference_count, 1, memory_order_relaxed);
//             // 上面两个relaxed,谁先执行都可以
//             //更新完后释放序列锁 ,这里不用CAS，因为奇数其他写不了这里不用比较原值，所以直接add
//             atomic_fetch_add_explicit(&stage->lock.seq_lock, 1, memory_order_release);
//             break;
//         }
//     } while(true);

//     block_alloc_t *ret = (block_alloc_t *)(stage->init_pos + current_used);
//     ret->ptr = stage->init_pos + current_used + sizeof(block_alloc_t);
//     ret->size = size;
//     ret->stage = stage;

//     return (block_alloc_t*)(stage->init_pos + current_used);
// }

bool stage_destroy(stage_t *stage)
{
    // 用了柔性数组所以直接一次free即可
    // 这里需要先判断是否可free，引用计数为0才可free
    if (stage == NULL)
    {
        printf("stage destroyed is NULL\n");
        exit(-1);
    }

    size_t ref;
    size_t seq;
    int loop = 5;
    do
    {

        seq = atomic_load_explicit(&stage->lock.seq_lock, memory_order_acquire);

        if (seq & 1)
        {
            continue;
        }

        ref = atomic_load_explicit(&stage->reference_count, memory_order_acquire);
        if (ref != 0)
        {
            printf("current ref is not zero");
            return false;
        }

        if (atomic_compare_exchange_strong_explicit(&stage->lock.seq_lock, &seq, seq + 1,
                                                    memory_order_acquire, memory_order_relaxed))
        {

            // 写保护，不会有其他线程在free之前对当前stage进行写操作，能够安全释放
            free(stage);
            atomic_fetch_add_explicit(&stage->lock.seq_lock, 1, memory_order_release);
            return true;
        }
    } while (--loop > 0);
    return false;
}

// 如果要实现，动态管理stage个数，那么reset和destroy应该嵌入到deref函数，最后一个的ref剪为0， 判断是去reset还是destroy
// 当然这里alloc失败，也要去判断能否reset，总体来看非常复杂
// 主要就是是否需要在J这里处理reset。单纯deref，直接ref--就行，然后让alloc失败的时候去reset
// 但是如果在deref这里处理reset，那么下一次alloc的时候就会少调用reset，并且deref肯定是一直都需要调用的
// 所以更好是在deref时，处理reset的情况。那么在什么情况下才需要reset呢
// 肯定是ref为0并且用了大量空间的时候,所以这个大量空间就需要选择合适的策略是满还是一半还是一半多
// 这里更改策略，如果按照先前alloc失败再去reset的话，首先可能有并发问题，当前alloc失败但是其他线程alloc成功，导致reset大概率白白调用，降低性能
// 并且就算reset成功还要再去调用alloc，alloc失败->reset->alloc,性能很低
// 现在让deref来决定是否reset。这样alloc失败不调用reset而是直接向后走，alloc失败空间不足 -> 下一个stage或者创建新stage
// deref内部直接实现reset这样有更高的并发，因为完全嵌合，保证原子值的一致性，而不是上面两个函数调用导致其他线程在之间改变原子值。
// 同时减少了调用reset失败的性能消耗.所以改用该策略让deref来reset.alloc失败直接向下走跳过当前stage

// 目前先保留至少128字节，如果当前字节少于128那么才调用reset
// 128能存储当前全部的结构，整形只需要4字节，指针只需要8字节用来指向各种数据结构。这里考虑到字符串所以128
bool stage_deref(stage_t *stage)
{
    // 这里ref通过sub返回旧值，如果旧值是1，当前值就是0，可以执行reset操作
    size_t seq, ref, used;
    // 这里同样需要获取锁，使得避免alloc过程中又reset的并发错误
    do
    {
        seq = atomic_load_explicit(&stage->lock.seq_lock, memory_order_acquire);
        // ref = atomic_load_explicit(&stage->reference_count, memory_order_acquire);
        // if (ref == 0) {
        //     printf("stage过度deref\n");
        //     exit(-1);
        // }

        if (seq & 1)
        {
            continue;
        }

        if (atomic_compare_exchange_weak_explicit(&stage->lock.seq_lock, &seq, seq + 1,
                                                  memory_order_acquire, memory_order_relaxed))
        {

            ref = atomic_load_explicit(&stage->reference_count, memory_order_relaxed);
            if (ref == 0)
            {
                printf("stage过度deref,程序结束\n");
                exit(-1);
            }

            ref = atomic_fetch_sub_explicit(&stage->reference_count, 1, memory_order_relaxed);
            if (ref == 1)
            {
                // 说明当前ref值为0,这里再去判断是否满足reset的条件这里获取used和capacity进行判断
                used = atomic_load_explicit(&stage->used, memory_order_relaxed);
                if (used >= stage->capacity - 128)
                {
                    // reset
                    atomic_store_explicit(&stage->used, 0, memory_order_relaxed);
                }
            }
            atomic_fetch_add_explicit(&stage->lock.seq_lock, 1, memory_order_release);
            return true;
        }
    } while (true);

    return false;
}

bool stage_deref_batch(stage_t *stage, size_t deref_cnt)
{

    size_t seq, used;
    size_t cur_ref;
    do
    {
        seq = atomic_load_explicit(&stage->lock.seq_lock, memory_order_acquire);

        if (seq & 1)
        {
            continue;
        }

        if (atomic_compare_exchange_weak_explicit(&stage->lock.seq_lock, &seq, seq + 1,
                                                  memory_order_acquire, memory_order_relaxed))
        {

            // 这里还是先锁再去判断
            // 这里判断是否i达到了阈值
            // 达到阈值和ref为0那么就可以直接reset重置
            // 否则只deref当前值
            // 这里还是要做安全处理
            // 检查是否过度deref了

            // 这里注意类型安全，不能用uint8_t,因为累计的值很可能>=256,当256时，cur_ref得到的值为0
            // 导致判断错误,所以需要使用尽可能大的类型，这里先用size_t
            // 如果超过取值范围下面的判断会直接报错终止程序
            
            cur_ref = atomic_load_explicit(&stage->reference_count, memory_order_relaxed);

            if (deref_cnt > cur_ref)
            {
                printf("deref_cnt : %ld, cur_ref : %ld\n", deref_cnt, cur_ref);
                perror("过度deref or 引用累计值超过类型取值范围 直接break\n");
                break;
                // deref_cnt = cur_ref; 
            }

            // 可以deref，未超过当前值

            cur_ref = atomic_fetch_sub_explicit(&stage->reference_count, deref_cnt, memory_order_relaxed);
            if (cur_ref == deref_cnt)
            {
                // 两个值相等，说明当前引用为0
                // 说明当前ref值为0,这里再去判断是否满足reset的条件这里获取used和capacity进行判断
                used = atomic_load_explicit(&stage->used, memory_order_relaxed);
                if (used >= stage->capacity - 128)
                {
                    // reset
                    atomic_store_explicit(&stage->used, 0, memory_order_relaxed);
                }
            }
            atomic_fetch_add_explicit(&stage->lock.seq_lock, 1, memory_order_release);
            return true;
        }
    } while (true);

    return false;
}

// 这里一般不使用直接的reset操作
bool stage_reset(stage_t *stage)
{

    // 有写操作，所以需要获取序列锁，获取之前，判断是否可写，和上面的差不多，整体结构差不多
    if (stage == NULL)
    {
        return NULL;
    }

    size_t seq, used, ref;
    // 先判断当前是否有写操作

    seq = atomic_load_explicit(&stage->lock.seq_lock, memory_order_acquire);
    if (seq & 1)
    {
        // continue;
        return false;
    }

    used = atomic_load_explicit(&stage->used, memory_order_acquire);
    ref = atomic_load_explicit(&stage->reference_count, memory_order_acquire);
    // ref不为0说明还有包没有释放，又不清楚包多久才能释放，所以直接返回false。选择下一个stage，如果每个stage都放不下
    // 且不能reset的话就直接创建新的stage,这样能有效避免while和CAS多次重试，性能极具下降
    if (ref != 0)
    {
        return false;
    }

    if (used == 0)
    {
        return true;
    }

    // used!=0,获取写锁,如果有线程改变了seq,那么必然进行了写操作,那么这里也是false直接返回
    if (atomic_compare_exchange_strong_explicit(&stage->lock.seq_lock, &seq, seq + 1,
                                                memory_order_acquire, memory_order_relaxed))
    {

        atomic_store_explicit(&stage->used, 0, memory_order_relaxed);

        atomic_fetch_add_explicit(&stage->lock.seq_lock, 1, memory_order_release);
        return true;
    }
    else
    {
        return false;
    }
}

// // block_alloc_t *stage_alloc(size_t size, stage_t *stage) {
//         // block_alloc_t *block = NULL;
//
//         //unfair size
//         // size_t used = atomic_load(&stage->used);
//
//         // 把元信息嵌入到分配块的头部
//         // 从当前used位置开始分配,used为偏移量
//         // 这里需要考虑到多个线程共同申请同一个stage
//         // 这里需要先预分配，然后出循环，再返回
//
//         // size_t new_used = used + block_size + size;
//         // if (new_used > stage->capacity) {
//         //     printf("new_used > stage->capacity\n");
//         //     return NULL;
//         // }
//         // while (!atomic_compare_exchange_weak(&stage->used, &used, new_used)) {
//         //     new_used = used + block_size + size;
//         //     if (new_used > stage->capacity) {
//         //         printf("new_used > stage->capacity\n");
//         //         return NULL;
//         //     }
//         // }
//         // 预分配成功，跳出循环，返回block块
//         // block = stage->init_pos + new_used - block_size - size;
//         // block->ptr = block + block_size;
//         // block->size = size;
//         // block->stage = stage;
//         // atomic_fetch_add(&stage->reference_count, 1);
//         // return block;
// // }
// // 这里涉及多个原子变量的更改，使用sequence lock
// //bool stage_reset(stage_t *stage) {
//     //重置当前Stage的used为0
//     // 这里需要检查是否符合重置条件
//     // 当refcount为0的话就重置成功，否则失败
//     //size_t reference_count = atomic_load(&stage->reference_count);
//     //do {

//         //if (reference_count != 0) {
//       //      printf("stage reset fal ied!\n");
//     //        return false;
//   //      }
//     // 这里涉及多个原子变量共同修改和同步，一种解决方法是使用一个数，例如128位，前64位是used,后64位是reference_count

// //    } while (atomic_compare_exchange_weak(&stage->reference_count, &reference_count, 0));
// // }

// // typedef struct stage_s {
// //     atomic_size_t used;
// //     size_t capacity;
// //     // refernce_count 用于判断是否可reset
// //     atomic_size_t reference_count;
// //     struct stage_s *next;
// //     uint8_t init_pos[];
// // } stage_t;
// // atomic_compare_exchange_weak(&stage->reference_count, &reference_count, 0)
// // 如果只有reference_count使用CAS，那么CAS的时候没有改变并不保证，当我下面更改used时为0.所以我需要保证同一时间两个原子变量的修改