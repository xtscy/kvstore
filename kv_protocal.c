#include "kv_protocal.h"
#include <string.h>
#include <poll.h>
#include "threadpool/threadpool.h"
#include "./NtyCo-master/core/nty_coroutine.h"

uint32_t read_to_ring(connection_t *c)
{
    //* 缓存区会比环形缓冲区大很多。这里外层用循环，填满然后处理消息
    printf("read_to_ring->\n");
    // RB_Write_String(&c->read_rb, c->read_cache, copy_byte);
    // 这里如果read_rb是满，那么将会死循环
    uint32_t freesize = RB_Get_FreeSize(&c->read_rb);
    // printf("", freesize);
    uint32_t copy_byte = c->read_cache.length - c->read_cache.head < freesize ?
        c->read_cache.length : freesize;
    printf("read_cache.length:%u,read_cache.head:%d, read_rb freesize->%u, copy_byte:%u\n",
        c->read_cache.length, c->read_cache.head, freesize, copy_byte);
    RB_Write_String(&c->read_rb, (uint8_t *)(c->read_cache.cache + c->read_cache.head), copy_byte);
    c->read_cache.head += copy_byte;
    return copy_byte;
}

//* 处理协议
//* 
void Process_Message(connection_t *c)
{
    while (1)
    {
        if (c->read_cache.head == c->read_cache.length)
        {
            printf("跳到最外层\n");
            return; //* 直接return结束循环
        }
        else
        {
            // printf("read_to_ring之前current KEY0:->%s\n", (char*)c->kv_array[0].key);
            read_to_ring(c);
            // printf("read_to_ring之后current KEY0:->%s\n", (char*)c->kv_array[0].key);
            Process_Protocal(c);
            // printf("Process_Protocal之后current KEY0:->%s\n", (char*)c->kv_array[0].key);

        }
    }
}

//* 4字节body长度前缀
//* body的长度不能超过RING_BUF_SIZE这是为了一个完整包的至少能存储在缓冲区中，这里是已经解决分包后，能够存储下来
//! 这里还有一个问题，就是如果对端丢包
//! 比如没有传输完body，后面如果又有header传来
//! 就会把header前缀数据误算作是body
//! 协议设计可以在header前面加个标识例如加个/r/n表示有新的请求包到来
//? 如果用的tcp，丢包的概率很小。如果对端发送了完整的数据包，没有收到回应就会一直重复发送
//? 而且如果访问量大，也就会频繁丢包，还是需要解决
//? 这个也是对端可能本身就是没有发送完整的数据包
//? 可以在每个包前面再加个\r\n来标识新的请求的开始
//? 方法1是直接丢弃前面的请求
//? 如果真的是丢包那么可以再设计一个包缓存，但是映射关系很难用某种方法去维护

// void smart_backoff_strategy() {

// }

int Process_Protocal(connection_t *c)
{
    //* 依次读到每个数据包，这里解决粘包和拆包问题
    uint32_t size = RB_Get_Length(&c->read_rb);
    //* 这里的结构体必须有读头部和读body的状态标志
    //* 因为可能被拆包，下一次进来不一定就是直接处理头部，所以需要判断
    while (1)
    {
        if (c->state == PARSE_STATE_HEADER) {
            //* 处理头部,可是头部的包也可能被拆分，如果不足4字节直接退出处理
            if (c->read_rb.Length < 4U) {
                printf("环形缓冲区header数据不足,read_rb.Length->%u\n", c->read_rb.Length);
                return -3;//* -3header未读取
            } else {
                printf("read header1, header:%u\n", c->current_header);
                //* 由于判断了长度有效，所以必然返回成功，除非出现了非代码错误
                RB_Read_String(&c->read_rb, (uint8_t*)&c->current_header, 4U);
                printf("current_header:%u\n", c->current_header);
                c->state = PARSE_STATE_BODY;
            }
        } else if (c->state == PARSE_STATE_BODY) {
            if (c->read_rb.Length < c->current_header) {
                // poll(NULL,NULL,5000);
                printf("环形缓冲区Body数据不足,回到上层再次从缓存区中读数据 \
                    当前所需要Body长度%u, 当前read_rb.Length%d\n", c->current_header, c->read_rb.Length);
                return -4;//* boby长度不足返回-4
            } else {

                //  task_t *task = (task_t*)malloc(sizeof(task_t));

                block_alloc_t block = allocator_alloc(&global_allocator, c->current_header + 1U);
                // 拿到block,然后写入线程的任务队列中
                if (block.ptr != NULL) {
                    bzero(block.ptr, block.size);// 多一个/0用于c语言字符串的结束标志
                    RB_Read_String(&c->read_rb, block.ptr, block.size - 1);
                    block.conn_fd = c->fd;
                    // block.size = c->current_header;
                    // char *temp_payload = malloc(c->current_header);
                    // if (temp_payload != NULL) {
                        // task->payload = temp_payload;
                        // RB_Read_String(&c->read_rb, task->payload, task->payload_len);
                        // if (LK_RB_Write_Task(&g_thread_pool.workers[worker_idx], task, 1) == RING_BUFFER_SUCCESS) {
                        //     printf("task input success\n");
                        //     break;
                        // }
                    // } else {
                        // printf("temp_payload malloc failed\n");
                        // exit(-1);
                    // }

                    //* LK_RB_Write_Task写入 失败,free掉temp_payload
                    // free(temp_payload);
                } else {
                    //* malloc 失败都直接退出
                    printf("block alloc failed from allocator\n");
                    exit(-1);
                }
                //* block开辟成功
                uint64_t usecd = INITIAL_BACKOFF_US;
                nty_schedule *sched = nty_coroutine_get_sched();
                if (sched == NULL) {
                    printf("scheduler not exsist!\n");
                    return -1;
                }
                nty_coroutine *co = sched->curr_thread;
                double backoff_us = INITIAL_BACKOFF_US;
                bool sign = false;
                while (1) {
                    //* 依次遍历线程
                    for (int i = 0; i < g_thread_pool.worker_count; i++) {
                    // g_thread_pool.workers[worker_idx]
                        int worker_idx = g_thread_pool.scheduler_strategy();
                        if (LK_RB_Write_Block(&g_thread_pool.workers[worker_idx].queue, &block, 1) == RING_BUFFER_SUCCESS) {
                            sem_post(&g_thread_pool.workers[worker_idx].sem);
                            sign = true;
                            break;//* input success, 退出循环处理下一个数据
                        }
                    }

                     if (sign) {
                        break;
                    }
                    //* 线程的任务队列不足，那么去到全局环形队列
                    //* 这里有多个全局环形队列，获取原子变量依次去遍历，都没有就考虑开辟新的全局队列
                    //* 如果队列已达上限，那么使用智能退避延迟策略
                    // * 这里用专门的线程来处理全局队列的任务
                    // global queue
                    int rn = -1;
                    for (int i = 0; i < g_thread_pool.current_queue_num; i++) {
                        int next_queue = atomic_fetch_add_explicit(&g_thread_pool.produce_next_queue_idx, 1, memory_order_release) % g_thread_pool.current_queue_num;
                        if (LK_RB_Write_Block(&g_thread_pool.global_queue[next_queue], &block, 1) == RING_BUFFER_SUCCESS) {
                            sem_post(&g_thread_pool.sem[next_queue]);
                            rn = atomic_load_explicit(&g_thread_pool.sleep_num, memory_order_acquire);
                            if (rn > 0) {
                                pthread_cond_signal(&g_thread_pool.global_cond);
                            }
                            sign = true;
                            break;
                            // printf("放入全局队列");
                        }
                    }
 
                    //? 如果连全局都放不下，那么就使用智能退避
                    //? 这里是协程框架所以直接用yield的而非sleep

                    if (sign) {
                        break;
                    }

                    
                    nty_schedule_sched_sleepdown(co, backoff_us);
                    backoff_us *= 2;
                }
          
                c->state = PARSE_STATE_HEADER;

//            //    printf("read_rb.Length:%u, current_head:%u\n", c->read_rb.Length, c->current_header);
//            //    memset(c->pkt_data, 0, sizeof(RING_BUF_SIZE));
//            //    RB_Read_String(&c->read_rb, c->pkt_data, c->current_header);
//            //    printf("处理pocket:%s\n", c->pkt_data);
//            //    if(Process_Task(c) == 0) {
//            //        printf("current KEY0:->%s\n", (char*)c->kv_array[0].key);
//            //        printf("1个完整的请求被处理\n");
//      // * 重新把payload放入当前连接的ring_buf
            //    // * 这里可能当前线程队列已满，返回ERROR
              //  // * 那么
                ///// * 写入任务队列后，线程拿取任务再执行
  //          //        memset(c->pkt_data, 0, RING_BUF_SIZE+1);
  //          //        printf("current KEY0:->%s\n", (char*)c->kv_array[0].key);
  //          //    }
  //          //    c->state = PARSE_STATE_HEADER;
  //          //    c->current_header = 0;
            }      
        }
    }
}

