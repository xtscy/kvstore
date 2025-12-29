#include "kv_protocal.h"
#include <string.h>
#include <poll.h>
#include <arpa/inet.h>  
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
        c->read_cache.length - c->read_cache.head : freesize;
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
        if (c->read_cache.head >= c->read_cache.length)
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
// extern uint32_t global_output_addr_prev;
int Process_Protocal(connection_t *c)
{
    //* 依次读到每个数据包，这里解决粘包和拆包问题
    // uint32_t size = RB_Get_Length(&c->read_rb);
    //* 这里的结构体必须有读头部和读body的状态标志
    //* 因为可能被拆包，下一次进来不一定就是直接处理头部，所以需要判断
    while (1)
    {
        // 按照resp协议的状态转换来读取请求
        // 一个请求就是一个数组类型组织的批量字符串
        // 这里先读取开头的数组长度和命令，放在一个阶段解析，然后做一些参数检查
        // 比如get命令的参数只能是1个，set命令的参数是两个
        // 这里检查完后,就去解析参数，然后构造成一个block，命令之间用空格分割
        // 可是这里使用的是批量字符串，支持了空格和\r\n但是如果按照空格分割，那么在真正的block被解析的时候
        // 其实还是不能支持空格的。所以底层的解析需要更改
        // 这里直接按照协议解析，然后如果是set那么这里直接调用set方法吗
        // 这里还是交给线程去执行任务，那么这里还是需要构造block
        // 但是内部结构需要发生改变，这里应该记录键的长度还有值得长度
        // 然后block->ptr指向一片空间，然后把键和值写到该空间
        // 然后线程的任务解析也需要修改
        // 这里线程读到一个block，这个block目前是只是一个请求，然后标记一下是set还是get
        // 但是这里也可以实现成一个批量写请求到一个block中，然后这个block被读取然后执行多个请求，这里节省了一个block的开销
        // 但是解析的耗时增加了，如果只是1个的话拿到该块，就能够去到对应的函数处理，处理完了就下一个
        // 但是如果有多个，那么就又需要一层协议解析，然后依次处理任务
        // 所以这里就先实现成,1个block对应1个请求，线程拿到直接执行就可以，这样也节省了多个任务再次解析的一个开销
        // 所以这里在参数个数和第一个命令参数读取完后，去到对应的分支构造block请求 ，然后放入到某个线程的队列中
        // 然后线程拿到block并且去执行相应的任务，然后发送对应的响应信息，当然这里如果请求错误
        // 那么就需要构造一个错误信息的block，然后线程拿到该错误信息请求，去响应对应的错误信息给客户端按照resp协议
        // 那么这里每用完一个block，就可以把当前block再回收，放入固定内存池
        // block的内容是在ptr中，这里的ptr就先不回收
        // 因为回收策略有点问题，再者这里如果stage不够用会去新开辟，所以可以先不考虑回收stage的ptr


        // 流式解析，这里是初始状态那么就去读
        // 这里先读取*类型，这里直接判断如果不是*那么说明发送的协议有问题
        // 因为命令都是按照数组的形式进行发送

        if (c->parser_stack.top == -1) {
            // 创建一个栈帧，用于处理数据
            
            
        } else {
            int temp_top = c->parser_stack.top;
            if (c->parser_stack.frames[temp_top].state == STATE_INIT) {


                
            } else if (c->parser_stack.frames[temp_top].state == STATE_READING_LINE) {

            } else if (c->parser_stack.frames[temp_top].state == STATE_EXPECTING_CR) {


                
            }
        }
        
        
        
        
        
        if (c->state == PARSE_STATE_HEADER) {
            //* 处理头部,可是头部的包也可能被拆分，如果不足4字节直接退出处理
            if (c->read_rb.Length < 4) {
                printf("环形缓冲区header数据不足,read_rb.Length->%u\n", c->read_rb.Length);
                return -3;//* -3header未读取
            } else {
                printf("read header1, header:%u\n", c->current_header);
                //* 由于判断了长度有效，所以必然返回成功，除非出现了非代码错误
                uint32_t pre_head = c->read_rb.head;
                if (RB_Read_String(&c->read_rb, &c->current_header, 4U) == RING_BUFFER_ERROR) {
                    printf("int Process_Protocal RB_Read_String(&c->read_rb, (void*)&c->current_header, 4U) == RING_BUFFER_ERROR)\n");
                    abort();
                    exit(-13);
                }
                c->current_header = ntohl(c->current_header);
                if (c->current_header >= 30) {
                    printf("c->current_header >= 30 exit\n");
                    abort();
                    exit(-13);
                }
                printf("current_header:%u\n", c->current_header);
                c->state = PARSE_STATE_BODY;
            }
        } else if (c->state == PARSE_STATE_BODY) {
            if (c->read_rb.Length < c->current_header) {
                // poll(NULL,NULL,5000);
                printf("环形缓冲区Body数据不足,回到上层再次从缓存区中读数据 \
                    当前所需要Body长度%u, 当前read_rb.Length%d\n", c->current_header, c->read_rb.Length);
                return -4;//* body长度不足返回-4
            } else {

                //  task_t *task = (task_t*)malloc(sizeof(task_t));

                block_alloc_t block = allocator_alloc(&global_allocator, c->current_header + 1U);

                // 拿到block,然后写入线程的任务队列中
                if (block.ptr != NULL) {
                    bzero(block.ptr, block.size);// 多一个/0用于c语言字符串的结束标志
                    if (RB_Read_String(&c->read_rb, block.ptr, c->current_header) == RING_BUFFER_ERROR) {
                        printf("int Process_Protocal RB_Read_String(&c->read_rb, (void*)&c->current_header, 4U) == RING_BUFFER_ERROR)\n");
                        abort();
                    }
                    printf("ptr->%s,size->%u\n", block.ptr, block.size);
                    printf("c->current_header:%u\n", c->current_header);
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
                    abort();
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
                        uint32_t worker_idx = g_thread_pool.scheduler_strategy();
                        if (LK_RB_Write_Block(&g_thread_pool.workers[worker_idx].queue, &block, 1) == RING_BUFFER_SUCCESS) {
                            if (sem_post(&g_thread_pool.workers[worker_idx].sem) != 0) {
                                printf("sem_post 失败 %s\n", strerror(errno));
                                abort();
                            }
                            sign = true;
                            break;//* input success, 退出循环处理下一个数据
                        } else {
                            printf("空间不足\n");
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
                    // int rn = -1;
                    // for (int i = 0; i < g_thread_pool.current_queue_num; i++) {
                    //     int next_queue = atomic_fetch_add_explicit(&g_thread_pool.produce_next_queue_idx, 1, memory_order_release) % g_thread_pool.current_queue_num;
                    //     if (LK_RB_Write_Block(&g_thread_pool.global_queue[next_queue], &block, 1) == RING_BUFFER_SUCCESS) {
                    //         sem_post(&g_thread_pool.sem[next_queue]);
                    //         rn = atomic_load_explicit(&g_thread_pool.sleep_num, memory_order_acquire);
                    //         if (rn > 0) {
                    //             pthread_cond_signal(&g_thread_pool.global_cond);
                    //         }
                    //         sign = true;
                    //         break;
                    //         // printf("放入全局队列");
                    //     }
                    // }
 
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

