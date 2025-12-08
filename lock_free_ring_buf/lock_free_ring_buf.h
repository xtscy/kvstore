#ifndef _LOCK_FREE_RING_BUFFER_H_
#define _LOCK_FREE_RING_BUFFER_H_

#include <stdatomic.h>
#include <stdint.h>
#include <stddef.h>
#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include "../ARENA_ALLOCATOR/stage_allocator.h"

//返回值定义
#define RING_BUFFER_SUCCESS     0x01
#define RING_BUFFER_ERROR       0x00

#define BUFFER_SIZE 1024

#define GET_HEAD(state) ((uint32_t)(state & 0xFFFFFFFF))
#define GET_TAIL(state) ((uint32_t)(state >> 32))
#define MAKE_STATE(head, tail) (((uint64_t)tail << 32 ) | head)

typedef struct task_s {
    int conn_fd;
    char* payload;
    size_t payload_len;
} task_t;




//环形缓冲区结构体
typedef struct
{
    //* 头指针指向数据，尾指针指向空。左闭右开
    //* 表示当前array的下标，每一个单位是block_alloc_t大小
    _Atomic(uint64_t) state;         //*低32位head， 高32位tail
    // uint32_t Length ;           //已储存的数据量
    uint32_t max_Length ;       //缓冲区最大可储存数据量
    block_alloc_t *block_array;       //缓冲区储存数组基地址
}lock_free_ring_buffer;
// typedef struct
// {
//     //* 头指针指向数据，尾指针指向空。左闭右开
//     _Atomic(uint64_t) state;         //*低32位head， 高32位tail
//     // uint32_t Length ;           //已储存的数据量
//     uint32_t max_Length ;       //缓冲区最大可储存数据量
//     task_t *task_array ;       //缓冲区储存数组基地址
// }lock_free_ring_buffer;

uint8_t LK_RB_Init(lock_free_ring_buffer  *rb_handle, uint32_t buffer_size);               //初始化基础环形缓冲区
uint8_t LK_RB_Delete(lock_free_ring_buffer *rb_handle, uint32_t Length);                                        //从头指针开始删除指定长度的数据
uint8_t LK_RB_Write_Block(lock_free_ring_buffer *rb_handle, block_alloc_t *input_addr, uint32_t write_Length);       //向缓冲区尾指针写指定长度数据
uint8_t LK_RB_Read_Block(lock_free_ring_buffer *rb_handle, block_alloc_t *output_addr, uint32_t read_Length);        //从缓冲区头指针读指定长度数据
uint32_t LK_RB_Get_Length(lock_free_ring_buffer *rb_handle);                                                    //获取缓冲区里已储存的数据长度
uint32_t LK_RB_Get_FreeSize(lock_free_ring_buffer *rb_handle);                                                  //获取缓冲区可用储存空间

#endif//#ifndef _RING_BUFFER_H_