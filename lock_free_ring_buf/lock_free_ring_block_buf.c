#include "lock_free_ring_buf.h"




uint8_t LK_RB_Init(lock_free_ring_buffer *rb_handle, uint32_t buffer_size)
{
    //缓冲区数组空间必须大于2且小于数据类型最大值
    if(buffer_size < 2 || buffer_size == 0xFFFFFFFF) {
        return RING_BUFFER_ERROR ; //初始化失败
    }

    //* 初始化head,tail,这里初始化让tail指向max_Length
    ///* 与普通的max_Length - 1区分开 
    atomic_store(&rb_handle->state, 0);
    uint32_t head = 0;
    uint32_t tail = rb_handle->max_Length;
    uint64_t state = MAKE_STATE(head, tail);
    atomic_exchange_explicit(&rb_handle->state, state, memory_order_relaxed);
    //* 开辟数组空间
    rb_handle->block_array = (block_alloc_t*)malloc(buffer_size * sizeof(block_alloc_t));
    if (rb_handle->block_array == NULL) {
        printf("LK_RB malloc failed\n");
        return RING_BUFFER_ERROR;
    }
    rb_handle->max_Length = buffer_size;
    return RING_BUFFER_SUCCESS ; //缓冲区初始化成功
}

//* 使用乐观并发控制来保证强一致性

uint8_t LK_RB_Delete(lock_free_ring_buffer *rb_handle, uint32_t Length)
{
    if (Length == 0) return RING_BUFFER_SUCCESS ;
    uint64_t old_state = 0, new_state = 0;
    uint32_t old_head = 0, old_tail = 0, new_head = 0, new_tail = 0;

    old_state = atomic_load(&rb_handle->state);
    do {
        old_head = GET_HEAD(old_state);
        old_tail = GET_TAIL(old_state);
        uint32_t num = 0;
        //* 计算个数
        if (old_head <= old_tail) {
            num = old_tail - old_head;
        } else {
            num = rb_handle->max_Length - old_head + old_tail;
        }

        if (num < Length) {
            return RING_BUFFER_ERROR ;
        }
        //* 开始删除计算新head
        if (old_head + Length < rb_handle->max_Length) {
            new_head = old_head + Length;
        } else {
            new_head = Length - (rb_handle->max_Length - old_head);
        }

        new_state = MAKE_STATE(new_head, old_tail);
        
    } while(!atomic_compare_exchange_strong(&rb_handle->state, &old_state, new_state));
    
    return RING_BUFFER_SUCCESS ;
}



//* 预留空间再拷贝
uint8_t LK_RB_Write_Block(lock_free_ring_buffer *rb_handle, block_alloc_t *input_addr, uint32_t write_Length)
{
    if (write_Length == 0) {
        return RING_BUFFER_SUCCESS ;
    }

    uint64_t old_state = 0, new_state = 0;
    uint32_t old_head = 0;
    uint32_t old_tail = 0, new_tail = 0;
    old_state = atomic_load(&rb_handle->state);
    
    do {
        old_head = GET_HEAD(old_state);
        old_tail = GET_TAIL(old_state);
        uint32_t count = 0, free_size = 0;
        // 计算当前个数，tail指向下一个写入位置
        if (old_tail == rb_handle->max_Length) {
            if (old_head != 0) {
                printf("state false\n");
                abort();
            }
            count = 0;
        } else {
            // 相等表示只有1个，满时old_tail不会等于old_head
            if (old_head <= old_tail) {
                count = old_tail - old_head + 1;
            } else {
                // tail指向数据存在的位置,左闭右闭的情况
                count = rb_handle->max_Length - old_head + old_tail + 1;
            }
        }
        free_size = rb_handle->max_Length - count;
        // 可以小于等于都返回RING_BUFFER_ERROR
        // 或者在state中多1位来表示是否满
        // 或者让tail指向可读的前一个位置，这样每次在增加的时候都需要移动tail
        // 如果是最后一个位置那么移到0，如果不是最后一个位置直接+1
        // 这样满时不会相等，初始化为-1,然后不是rb_handle->max_Length,那么+1
        if (free_size < write_Length) {
            return RING_BUFFER_ERROR ;
        }

        // 向old_tail的下一个位置进行写入，先做判断移动tail
        if (old_tail < rb_handle->max_Length - 1) {
            old_tail++;
        } else {
            old_tail = 0;
        }
        // if (old_tail == rb_handle->max_Length - 1) {

        // } else if (old_tail == rb_handle->max_Length) {
        //     //init
        //     old_tail
        // } else {

        // }
        
        if (rb_handle->max_Length - old_tail < write_Length) {
            new_tail = write_Length - (rb_handle->max_Length - old_tail) - 1;
        } else {
            new_tail = old_tail + write_Length - 1;
        }
        
        new_state = MAKE_STATE(old_head, new_tail);
    } while(!atomic_compare_exchange_weak(&rb_handle->state, &old_state, new_state));
    // 这里还没实现批处理的内存拷贝，目前只能拷贝1个
    memcpy(&rb_handle->block_array[old_tail], input_addr, write_Length * sizeof(block_alloc_t));
    
    return RING_BUFFER_SUCCESS;

}

//* 先读取数据再CAS判断，如果成功再返回函数，否则重新读取数据，每次有1个线程返回成功
uint8_t LK_RB_Read_Block(lock_free_ring_buffer *rb_handle, block_alloc_t *output_addr, uint32_t read_Length) {

    uint64_t old_state = 0, new_state = 0;
    old_state = atomic_load(&rb_handle->state);
    uint32_t old_head = 0, new_head = 0;
    uint32_t old_tail = 0;

    do {
        old_head = GET_HEAD(old_state);
        old_tail = GET_TAIL(old_state);
        
        uint32_t num = 0;
        //   // 相等的情况有可能是全满，也有可能没有值，
        // 左闭右闭，相等时不满，当满时不相等
        // 计算当前剩余值，
        if (old_tail == rb_handle->max_Length) {
            if (old_head != 0) {
                printf("state false\n");
                abort();
            }
            num = 0;
        } else {
            // 相等表示只有1个，满时old_tail不会等于old_head
            if (old_head <= old_tail) {
                num = old_tail - old_head + 1;
            } else {
                // tail指向数据存在的位置,左闭右闭的情况
                num = rb_handle->max_Length - old_head + old_tail + 1;
            }
        }

        if (num < read_Length) {
            return RING_BUFFER_ERROR ;
        }
//head 指向下一个可读位置，tail指向下一个可写位置的前一个位置
        // 让new_head指向下一个可读的位置
        if (rb_handle->max_Length - old_head > read_Length) {
            new_head = old_head + read_Length;
        } else {
            new_head = read_Length - rb_handle->max_Length + old_head;
        }
        memcpy(output_addr, &rb_handle->block_array[old_head], read_Length * sizeof(block_alloc_t));
        new_state = MAKE_STATE(new_head, old_tail);
    } while (!atomic_compare_exchange_weak(&rb_handle->state, &old_state, new_state));

    return RING_BUFFER_SUCCESS ;
    
}

//* GET 大小实现为粗略旧值不使用CAS

uint32_t LK_RB_Get_Length(lock_free_ring_buffer *rb_handle) {
    uint64_t old_state = atomic_load(&rb_handle->state);
    uint32_t old_head = GET_HEAD(old_state);
    uint32_t old_tail = GET_TAIL(old_state);
    uint32_t count = 0;

    if (old_tail == rb_handle->max_Length) {
        if (old_head != 0) {
            printf("state false\n");
            abort();
        }
        count = 0;
        return count;
    } else {
        // 相等表示只有1个，满时old_tail不会等于old_head
        if (old_head <= old_tail) {
            count = old_tail - old_head + 1;
        } else {
            // tail指向数据存在的位置,左闭右闭的情况
            count = rb_handle->max_Length - old_head + old_tail + 1;
        }
        return count;
    }
    abort();
}

uint32_t LK_RB_Get_FreeSize(lock_free_ring_buffer *rb_handle) {

    uint64_t old_state = atomic_load(&rb_handle->state);
    uint32_t old_head = GET_HEAD(old_state);
    uint32_t old_tail = GET_TAIL(old_state);
    uint32_t count = 0;


    if (old_tail == rb_handle->max_Length) {
        if (old_head != 0) {
            printf("state false\n");
            abort();
        }
        count = 0;
    } else {
        // 相等表示只有1个，满时old_tail不会等于old_head
        if (old_head <= old_tail) {
            count = old_tail - old_head + 1;
        } else {
            // tail指向数据存在的位置,左闭右闭的情况
            count = rb_handle->max_Length - old_head + old_tail + 1;
        }
    }
    return rb_handle->max_Length - count;
}