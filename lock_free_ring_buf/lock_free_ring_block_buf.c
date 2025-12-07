#include "lock_free_ring_buf.h"




uint8_t LK_RB_Init(lock_free_ring_buffer *rb_handle, uint32_t buffer_size)
{
    //缓冲区数组空间必须大于2且小于数据类型最大值
    if(buffer_size < 2 || buffer_size == 0xFFFFFFFF) {
        return RING_BUFFER_ERROR ; //初始化失败
    }

    //* 初始化head,tail    
    atomic_store(&rb_handle->state, 0);

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

    do {
        old_state = atomic_load(&rb_handle->state);
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
    old_head = GET_HEAD(old_state);
    old_tail = GET_TAIL(old_state);

    do {
        uint32_t count = 0, free_size = 0;
        if (old_head <= old_tail) {
            count = old_tail - old_head;
        } else {
            count = rb_handle->max_Length - old_head + old_tail;
        }
        free_size = rb_handle->max_Length - count;
        if (free_size < write_Length) {
            return RING_BUFFER_ERROR ;
        }

        if (rb_handle->max_Length - old_tail <= write_Length) {
            new_tail = write_Length - rb_handle->max_Length + old_tail;
        } else {
            new_tail = old_tail + write_Length;
        }
        
        new_state = MAKE_STATE(old_head, new_tail);
        
    } while(atomic_compare_exchange_strong(&rb_handle->state, &old_state, new_state));

    memcpy(&rb_handle->block_array[old_tail], input_addr, write_Length * sizeof(block_alloc_t));
    
    return RING_BUFFER_SUCCESS;

}

//* 先读取数据再CAS判断，如果成功再返回函数，否则重新读取数据，每次有1个线程返回成功
uint8_t LK_RB_Read_Block(lock_free_ring_buffer *rb_handle, block_alloc_t *output_addr, uint32_t read_Length) {

    uint64_t old_state = 0, new_state = 0;
    old_state = atomic_load(&rb_handle->state);
    uint32_t old_head = GET_HEAD(old_state), new_head = 0;
    uint32_t old_tail = GET_TAIL(old_state);

    do {

        uint32_t num = 0;
        if (old_head <= old_tail) {
            num = old_tail - old_head;
        } else {
            num = rb_handle->max_Length - old_head + old_tail;
        }

        if (num < read_Length) {
            return RING_BUFFER_ERROR ;
        }

        if (rb_handle->max_Length - old_head > read_Length) {
            new_head = old_head + read_Length;
        } else {
            new_head = read_Length - rb_handle->max_Length + old_head;
        }
        memcpy(output_addr, &rb_handle->block_array[old_head], read_Length * sizeof(block_alloc_t));
        new_state = MAKE_STATE(new_head, old_tail);
    } while (atomic_compare_exchange_strong(&rb_handle->state, &old_state, new_state)) ;


    return RING_BUFFER_SUCCESS ;
    
}

//* GET 大小实现为粗略旧值不使用CAS

uint32_t LK_RB_Get_Length(lock_free_ring_buffer *rb_handle) {
    uint64_t old_state = atomic_load(&rb_handle->state);
    uint32_t old_head = GET_HEAD(old_state);
    uint32_t old_tail = GET_TAIL(old_state);
    if (old_head <= old_tail) {
        return old_tail - old_head ;
    } else {
        return rb_handle->max_Length - old_head + old_tail ;
    }
}

uint32_t LK_RB_Get_FreeSize(lock_free_ring_buffer *rb_handle) {

    uint64_t old_state = atomic_load(&rb_handle->state);
    uint32_t old_head = GET_HEAD(old_state);
    uint32_t old_tail = GET_TAIL(old_state);
    uint32_t num = 0;

    
    if (old_head <= old_tail) {
        num =  old_tail - old_head ;
    } else {
        num =  rb_handle->max_Length - old_head + old_tail ;
    }
    
    return rb_handle->max_Length - num ;

}