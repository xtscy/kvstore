
#include "kv_protocal.h"
#include <string.h>
#include <poll.h>

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
            printf("read_to_ring之前current KEY0:->%s\n", (char*)c->kv_array[0].key);
            read_to_ring(c);
            printf("read_to_ring之后current KEY0:->%s\n", (char*)c->kv_array[0].key);
            //* 当把数据读到环形缓冲区后，在该环形缓冲区中处理数据
            Process_Protocal(c);
            printf("Process_Protocal之后current KEY0:->%s\n", (char*)c->kv_array[0].key);

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


void Process_Protocal(connection_t *c)
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
                return;
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
                return;
            } else {
                printf("read_rb.Length:%u, current_head:%u\n", c->read_rb.Length, c->current_header);
                memset(c->pkt_data, 0, sizeof(RING_BUF_SIZE));
                RB_Read_String(&c->read_rb, c->pkt_data, c->current_header);
                printf("处理pocket:%s\n", c->pkt_data);
                if(Process_Task(c) == 0) {
                    printf("current KEY0:->%s\n", (char*)c->kv_array[0].key);
                    printf("1个完整的请求被处理\n");

                    memset(c->pkt_data, 0, RING_BUF_SIZE+1);
                    printf("current KEY0:->%s\n", (char*)c->kv_array[0].key);

                }
                c->state = PARSE_STATE_HEADER;
                c->current_header = 0;
            }      
        }
    }
}

