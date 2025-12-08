
                    //* 这里未成功放入，那么有一个策略就是重新添加到当前连接的ringbuf中，然后把上面malloc的全部都free
                    //* 那么这里如果直接重新添加，就是添加到环形队列的末尾,由于是从当前连接取出来的，所以必然能够重新插入
                    //* 这里为了保证，把当前任务push进去。则如果当前线程队列不足就换其他线程。
                    //* 如果所有线程都没有空间，那么设计一个全局任务队列。
                    //* 把不能放入的任务放到该全局队列。该全局队列在内存池中申请，这里先设计为malloc。
                    //* 该全局队列用无锁环形队列，因为是共享的，考虑线程安全性.
                    //* 如果当前全局任务队列为满，那么再去开辟一个新的全局任务队列。这里后续优化可能涉及到某个时刻可能
                    //* 会有巨大的请求数据，导致开辟了许多全局任务队列，但是当过了这个峰值。那么就会有多个冗余的全局队列占用内存
                    //* 那么优化就可以实现对于一个任务都没有的冗余队列的清理。设置一个一般情况下，最多有几个冗余队列。
                    //* 然后对于超出数量的队列，进行清理.当然这里清理也有策略，可以1个1个的清理这样可以做到
                    //* 在时间段相差不大的情况下，又有巨大并发数据，那么向内存池申请的次数就会减少，如果内存池不够，向系统申请就会减少

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
    uint32_t copy_byte = c->read_cache.length - c->read_cache.head < RB_Get_FreeSize(&c->read_rb) ?
        c->read_cache.length : RB_Get_FreeSize(&c->read_rb);
    printf("read_cache.length:%u,read_cache.head:%d, copy_byte:%u\n",
        c->read_cache.length, c->read_cache.head, copy_byte);
    RB_Write_String(&c->read_rb, (uint8_t *)(c->read_cache.cache + c->read_cache.head), copy_byte);
    c->read_cache.head += copy_byte;
    return copy_byte;
}

//* 处理协议

void Process_Message(connection_t *c)
{
    while (1)
    {
        if (c->read_cache.head == c->read_cache.length)
        {
            //* 静态缓存区已经被读完,跳到最外层循环重新调用recv
            printf("跳到最外层\n");
            return; //* 直接return结束循环
        }
        else
        {
            //* 静态缓存区存在数据那么继续处理
            // printf("read_to_ring之前current KEY0:->%s\n", (char*)c->kv_array[0].key);
            read_to_ring(c);
            // printf("read_to_ring之后current KEY0:->%s\n", (char*)c->kv_array[0].key);
            //* 当把数据读到环形缓冲区后，在该环形缓冲区中处理数据
            //* 这里设计成，让处理函数内部来实现保证正确写入
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
            if (c->read_rb.Length < 4) {
                printf("环形缓冲区header数据不足\n");
                return -3;//* -3header未读取
            } else {
                printf("read header1, header:%u\n", c->current_header);
                //* 由于判断了长度有效，所以必然返回成功，除非出现了非代码错误
                RB_Read_String(&c->read_rb, (uint8_t*)&c->current_header, sizeof(uint32_t));
                printf("current_header:%u\n", c->current_header);
                c->state = PARSE_STATE_BODY;
            }
        } else if (c->state == PARSE_STATE_BODY) {
            if (c->read_rb.Length < c->current_header) {
                // poll(NULL,NULL,5000);
                printf("环形缓冲区Body数据不足,回到上层再次从缓存区中读数据");
                return -4;//* boby长度不足返回-4
            } else {
                //* 如果读到BODY，那么去调用调度器，写入某个线程的队列中
                //* 这里就不再是调用Process_Task
                //* 而是调用调度器，和写入线程任务队列
                //* 这里封装函数就是线程安全的，并且一定会加1
                //* 这里有一个策略是无限循环，直到input到任务队列，然后会break
                //* 但是这时该协程就会一直占用，直到1个时间轮
                //* 目前是如果遍历了1遍，没有线程，那么使用共享任务队列
                //* 这里先就让调度器+1,循环的遍历，不改变原子变量
                //* 如果为了强轮询一致性，那么就需要持续的改变原子变量
                //* 这里其实如果说不改变原子，那么当前线程满了后，这个线程push不进去，另外一个线程的原子变量指向这个照样push


                //  task_t *task = (task_t*)malloc(sizeof(task_t));

                block_alloc_t block = allocator_alloc(&global_allocator, c->current_header + 1);
                // 拿到block,然后写入线程的任务队列中
                if (block.ptr != NULL) {
                    bzero(block.ptr, block.size);// 多一个/0用于c语言字符串的结束标志
                    block.conn_fd = c->fd;
                    // block.size = c->current_header;
                    // char *temp_payload = malloc(c->current_header);
                    // if (temp_payload != NULL) {
                        // task->payload = temp_payload;
                        RB_Read_String(&c->read_rb, block.ptr, block.size - 1);
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

