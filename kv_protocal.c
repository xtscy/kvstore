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
        //? 这里解析到每一个字符串然后直接放进数组中
        //? 对于嵌套数组也是如此，把真实的数据放到array中，这里是指针指向arena

        // 这里外层只需要初始化top值为-1就行
        if (c->parser_stack.top == -1) {
            // 使用一个栈帧，用于处理数据
            // 初始化字段即为创建，对象的真实创建在c的malloc时已经创建
            c->parser_stack.top = 0;
            c->parser_stack.frames.state = STATE_INIT;
        } else {
            // 拿到当前所处的栈帧
            if (c->parser_stack.frames[temp_top].state == STATE_INIT) {
                // *当前栈帧是INIT状态  
                //* 这里第一个类型必须是数组否则报错，但是这里还要考虑如果嵌套的情况
                //* 如果是嵌套那么可以不用读到*而是其他类型
                //* 所以这里的最外层读取和嵌套读取的状态是不同的这里应该区分开
                //* 如果有嵌套情况存在那么对应的状态设置为
                //* 这里需要先读取*类型和元素长度,当然可能缓冲区数据不够，这里类型说明都是1个字节
                //*所以这里直接判断长度是否有1个字节
                //* 这里是最外层的数组初始化
                if (c->read_rb.Length < 1) {
                    return -3;
                } else {
                    char temp_c = 0;
                    if (RB_Read_String(&c->read_rb, &temp_c, 1) == RING_BUFFER_ERROR) {
                        abort();
                        exit(-21);
                    }
                    // 读到类型，然后设置帧类型和当前读取状态
                    // 这里做字符匹配,根据不同的类型
                    // 设置不同的下一个外层状态和帧类型
                    // 基本类型
                    // if (temp_c == '+' || temp_c == '-' || temp_c == ':') {

                    //     c->parser_stack.frames[temp_top].ftype = FRAME_SIMPLE;
                    //     // 这里是简单类型，那么下一步就是读取行数据
                    //     c->parser_stack.frames[temp_top].state = STATE_READING_LINE;
                    //     c->parser_stack.frames[temp_top].received_len = 0;

                    //* 简单类型没有期望长度,直接读到\r\n为止
                    //     //* 那么这里状态实际上不是读取行数据,而是直接设置成读取\r\n
                    //     //* 因为并不知道需要读取多少行数据
                    // }
                    if (temp_c == '*') {
                        //去读取元素个数
                        // 正确读取到数组状态,设置读取数组长度的状态
                        c->parser_stack.frames.ftype = FRAME_ARRAY;
                        c->parser_stack.frames.state = STATE_ARRAY_READING_LENGTH;
                        c->parser_stack.line.expected_end = 0;
                        c->parser_stack.line.line_pos = 0;
                        memset(c->parser_stack.line.line_buffer, 0, MAX_LINE_BUFFER);

                        memset(&c->parser_stack.array, 0, sizeof(c->parser_stack.array));
                        c->parser_stack.array.mode = MODE_INLINE_ONLY;
                    } else {
                       c->is_back = true;
                       close(fd);
                       return -3;
                    }
                }

            }  else if (c->parser_stack.frames.state == STATE_ARRAY_READING_LENGTH) {
                //* 读取长度，这里长度直到读到\r\n
                //* 这里为了尽可能少的去低效率的解析
                //* 则有一个行数据缓冲区，在要解析数据时，先去行数据缓冲区
                //* 然后再去到环形队列中去读取
                //* 这里先判断哪一分支
                int read_byte = 0;
                if (c->read_rb.Length >= 64) {
                    read_byte = 64;
                } else if (c->read_rb.Length >= 32) {
                    read_byte = 32;
                } else if (c->read_rb.Length >= 16) {
                    read_byte = 16;
                } else if (c->read_rb.Length >= 8) {
                    read_byte = 8;
                } else if (c->read_rb.Length >= 4) {
                    read_byte = 4;
                } else if (c->read_rb.Length >= 2) {
                    read_byte = 2;
                } else if (c->read_rb.Length >= 1) {
                    read_byte = 1;
                } else {
                    return -3;                    
                }

                
                if (expected_end > MAX_LINE_BUFFER - read_byte) {
                    // 当前长度过长,直接断开连接
                    // 这里再设置个标记位上层循环先判断连接是否关闭如果关闭依次返回
                    // 返回到hook的recv然后直接退出该协程
                    c->is_back = true;
                    close(c->fd);
                    return;
                }
                if (RB_Read_String(&c->read_rb, c->parser_stack.line.line_buffer + c->parser_stack.line.line_pos, read_byte) == RING_BUFFER_ERROR) {
                    abort();
                    exit(-21);
                }                   
                // line_pos是当前解析的位置，expected_end是当前数据的末尾
                expected_end += read_byte;
                // 读到了新数据，while解析数据，这里有可能读到\r\n后面的数据
                // 这里读取到数据后必然去到下面解析
                //这里如果包含了\r\n必然读取到这个才退出
                // 否则退出说明当前数据\r\n还未包含，这个退出表示当前的数据全部解析完了
                while (true) {
                    //先判断line_pos是否到达了末尾，如果到达了末尾直接退出
                    if (c->parser_stack.line.line_pos == c->parser_stack.line.expected_end) {
                        // 当前line中的数据全部解析完，但是没有完整的\r\n
                        // 这里可以多加一个标志位，但是这里也可以判断最后一个是否是\r
                        // 先直接判断是否是\r如果是那么保留1个字符
                        // 下面如果读取到\r直接break了，如果是其他非法字符直接关闭
                        // 所以这里只有合格字符并且当前已经读到了末尾，那么这里直接重置状态
                        c->parser_stack.line.line_pos = 0;
                        c->parser_stack.line.expected_end = 0;
                        break;
                    }
                    // if (c->parser_stack.line_buffer[c->parser_stack.line.line_pos] == '\n') {
                    //     if ( c->parser_stack.line_buffer[c->parser_stack.line.line_pos] > 0 &&
                    //         c->parser_stack.line_buffer[c->parser_stack.line.line_pos - 1] == '\r') {
                    //         // 长度读取到\r\n,读取完，设置下一状态
                    //         c->parser_stack.frame.state = STATE_ARRAY_READING_ELEMENTS;
                    //         break;
                    //     } else {
                            
                    //         //当前是读取长度，但是却发送了\n无效字符，直接关闭连接，回退
                    //         // 设置回退标志
                    //         c->is_back = true;
                    //         close(c->fd);
                    //         return -3;
                    //     }
                    // }
                    // *两种，读\r\n再解析
                    // *2. 用uint读1解析1
                    // 对于数组字符
                    char cur_char = c->parser_stack.line.line_buffer[c->parser_stack.line.line_pos];

                    if (cur_char >= '0' && cur_char <= '9') {
                        int cur_num = cur_char - '0';
                        // 这里内联直接使用的是inline_count
                        // 如果是动态那么个数存储在expected_count中
                        if (c->parser_stack.array.mode == MODE_INLINE_ONLY) {

                            if (c->parser_stack.array.inline_count > 255 / 10) {
                                // 不能除10直接转动态
                                // 大于静态大小，去开辟动态数组，这里没有数据，因为数组元素
                                // 永远是读取完长度后才去读取数组元素的数据
                                c->parser_stack.array.mode = MODE_HYBRID;
                                c->parser_stack.array.expected_count = c->parser_stack.array.inline_count;
                                c->parser_stack.array.expected_count *= 10;
                                c->parser_stack.array.expected_count += cur_num;
    
                            } else {
                                //可以除10
                                int temp_cnt = c->parser_stack.array.inline_count * 10;
                                if (temp_cnt > 255 - cur_num) {
                                    // 转为动态
                                    c->parser_stack.array.mode = MODE_HYBRID;
                                    // 元素数这里最大不能超过size_t
                                    c->parser_stack.array.expected_count = temp_cnt + cur_num;
                                } else {
                                    // 静态足够
                                    // c->parser_stack.array.inline_count
                                    c->parser_stack.array.inline_count = temp_cnt + cur_num;
                                }
    
                            }
                            c->parser_stack.array.inline_count 
                        } else {
                            // MODE_HYBRID
                            // 对于动态数组，直接增加计数即可
                            c->parser_stack.array.expected_count = c->parser_stack.array.expected_count * 10 + cur_num;
                        }
                    } else if (c == '\r') {
                        // 设置状态为期望回车\n,这里直接break
                        // if (c->parser_stack.line.line_buffer[c->parser_stack.line.expected_end - 1] == '\r') {
                        //     // 保存\r到第一个字符
                        //     c->parser_stack.line.line_buffer[0] = '\r';
                        //     c->parser_stack.line.line_pos = 0;
                        //     c->parser_stack.line.expected_end = 1;
                        // } else {
                        //     c->parser_stack.line.line_pos = 0;
                        //     c->parser_stack.line.expected_end = 0;
                        // }
                        c->parser_stack.frames.state = STATE_EXPECTING_ARRAY_LENGTH_LF;
                        c->parser_stack.line.line_pos++;
                        break;
                    } else {
                        // 非法字符，直接错误信息设置回退，然后关闭,这里只能读到数字字符和\r
                        c->is_back = true;
                        close(c->fd);
                    }

                    ++c->parser_stack.line.line_pos;
                }

            } else if (c->parser_stack.frames.state == STATE_ARRAY_READING_ELEMENTS) {
            // else if (c->parser_stack.frames.state == STATE_READING_LINE) {
                //外层没有line状态,这里可以直接去掉,因为都是走的数组状态
                // 内层才是读取数据的状态
                // 基于协议，服务器接收到的都是被数组封装的请求
            // }
                //* 去读取每个元素，这里每个元素都是一个类型
                //* 那么在读取特定简单类型或者是批量字符串类型时
                //* 这里这个类型如果直接存储则会直接覆盖掉原本的数组类型
                //* 所以这里外层类型还是数组类型不动，然后在数组元素读取内部
                //* 存储的类型，这里解析元素的操作写在该STATE_ARRAY_READING_ELEMENTS内
                //* 而不是外层的frames.state的判断
                //* 这里如果数据不完整，那么继续跳到外层读取，然后state状态依然走该分支
                //* 然后继续读取.即外层依然是读取元素类型
                //* 外层也不需要简单类型的读取，因为发送的命令都是基于数组的
                //* 所以外层是大的数组状态，内层是简单类型和批量字符串类型的处理

                //* 这里写内层类型的状态迁移
                //* 这里在读取前肯定需要,如果是动态数组那么肯定需要动态开辟
                //* 并且如果是静态数组，那么做一定的初始化工作，所以这里加1个
                //* 数组读取值得初始化状态
                //* 这里初始化完毕，去进行
                //* 这里判断当前元素的一个读取状态
                //* 因为这里可能在某一状态数据不足从而退出
                //* 再次进入也需要判断当前处理的是哪一种类型，不同类型不同分支
                //* 所以这里有两个标志，一个用于当前外层状态，一个用于判断当前处理的是哪种类型不同类型不同分支

                if (c->parser_stack.array.current_state == STATE_READING_TYPE) {
                    //* 外层判断类型，之后之后先按类型分
                    //* 这里下一个字节的读取先判断line的缓冲区如果没有再去环形缓冲区中
                    //* 这里如果没数据则去读取，有数据不读取
                    if (c->parser_stack.line.line_pos == c->parser_stack.line.expected_end) {
                        //* 当前没有数据，从环形缓冲区中去拿
                        //* 这里调用封装的取数据的函数,并且如果该缓冲区也没有数据
                        //* 如果环形缓冲区也没有，则直接return,返回到上层重新读取数据
                     
                    }
                    // 现在肯定有数据
                    char temp_type = c->parser_stack.line.line_buffer[c->parser_stack.line.line_pos];
                    if (c->parser_stack.line.line_pos + 1 == c->parser_stack.line.expected_end) {
                        c->parser_stack.line.line_pos = 0;
                        c->parser_stack.line.expected_end = 0;
                    } else {
                        c->parser_stack.line.line_pos ++;
                    }
                    // 先对line_pos进行了处理，然后再去判断当前的类型
                    if (temp_type == '$') {
                        //设置内层的ftype
                        // 批量字符串设置读取长度
                        c->parser_stack.array.current_type = FRAME_BULK;
                        c->parser_stack.array.current_state = STATE_BULK_READING_LENGTH;
                        memset(c->parser_stack.bulk, 0, sizeof(c->parser_stack.bulk));
                    } else if (temp_type == '+' || temp_type == ':') {
                        // 简单类型直接去读数据直到\r然后设置对应的读取LF状态
                        // 这里解析了一个元素，需要放入数组中，那么这里怎么知道我要放入数组了呢
                        // 对于简单类型而言在读\n的标志中，说明读取完成，然后添加到数组中
                        // 对于批量字符串而言，在读取数据的\n标志中读取到，说明当前批量字符串
                        // 处理完成，同样增加到数组中
                        // 这里两种状态可以合并到同一个判断中进行读取
                        // 1个if多个\n判断都进去，然后再判断当前的标志是什么
                        // 从而进一步设置下一步的标志是什么
                        if (temp_type == '+') {
                            c->parser_stack.array.current_type = FRAME_SIMPLE_CHAR;
                        } else {
                            c->parser_stack.array.current_type = FRAME_INT;
                        }
                        c->parser_stack.array.current_state = STATE_READING_LINE;

                        
                    } else {
                        // 当前字符错误, 直接设置回退标志，并且关闭连接，后续优化可以设置错误信息到c->error_buf中
                        c->is_back = true;
                        close(c->fd);
                        return -3;
                    }

                    
                } else if (c->parser_stack.array.current_type == FRAME_INT || c->parser_stack.array.current_type == FRAME_SIMPLE_CHAR) {
                    // * 这里是对于简单类型的扩展
                    //* 客户端对于参数都会解析成批量字符串
                    //* 内层在判断当前类型的状态
                    //* 这里的缓冲区还是使用line中的缓冲区
                    //* 这里区分类型，以便于构建数据块，虽然目前数据结构只实现了存储整形的情况
                    //* 这里对于简单类型都是相同的state设置
                    if (c->parser_stack.line.line_pos == c->parser_stack.line.expected_end) {
                        //* 当前没有数据，从环形缓冲区中去拿
                        //* 这里调用封装的取数据的函数,并且如果该缓冲区也没有数据
                        //* 如果环形缓冲区也没有，则直接return,返回到上层重新读取数据
                     
                    }
                    if (c->parser_stack.array.current_state == STATE_READING_LINE) {
                        // 这里去line_buf中读取数据,直到读到\r
                        while (true) {
                            //* 这里有数据直接去区域中读取
                            char c 
                        }
                    } else if (c->parser_stack.array.current_state == STATE_READING_LINE_LF) {
                        //* 读取\n状态  
                        //* 这里表示上一个字符已经读取到\r,这里必须是\n因为这里的类型是简单类型,非二进制安全
                        
                        
                    }
                    

                } else if (c->parser_stack.array.current_type == FRAME_BULK) {
                    //* 同样先去保证一定有数据可读
                    if (c->parser_stack.line.line_pos == c->parser_stack.line.expected_end) {

                        int read_byte = 0;
                        if (c->read_rb.Length >= 64) {
                            read_byte = 64;
                        } else if (c->read_rb.Length >= 32) {
                            read_byte = 32;
                        } else if (c->read_rb.Length >= 16) {
                            read_byte = 16;
                        } else if (c->read_rb.Length >= 8) {
                            read_byte = 8;
                        } else if (c->read_rb.Length >= 4) {
                            read_byte = 4;
                        } else if (c->read_rb.Length >= 2) {
                            read_byte = 2;
                        } else if (c->read_rb.Length >= 1) {
                            read_byte = 1;
                        } else {
                            //* 环形队列也没有数据了，那么去到外层的缓冲区数组拿数据
                            //* 所以这里直接return返回到上层函数
                            return -3;
                        }
                        if (expected_end > MAX_LINE_BUFFER - read_byte) {
                            // 当前长度过长,直接断开连接
                            // 这里再设置个标记位上层循环先判断连接是否关闭如果关闭依次返回
                            // 返回到hook的recv然后直接退出该协程
                            c->is_back = true;
                            close(c->fd);
                            return;
                        }
                        if (RB_Read_String(&c->read_rb, c->parser_stack.line.line_buffer + c->parser_stack.line.line_pos, read_byte) == RING_BUFFER_ERROR) {
                            abort();
                            exit(-21);
                        }   
                        // line_pos是当前解析的位置，expected_end是当前数据的末尾
                        c->parser_stack.line.expected_end += read_byte;
                    }
                    //* 先判断当前的批量字符串的读取状态
                    if (c->parser_stack.array.current_state == STATE_BULK_READING_LENGTH) {
                        //* 这里去读取到长度字符，然后累计到bulk的expected期望总大小上
                        //* 并且如果大于128则使用动态
                        //* 这里则是读取完整体大小后，在LENGTH_LF中来设定是内联还是动态，并且进行一定的初始化工作
                        while (true) {
                            if (c->parser_stack.line.line_pos == c->parser_stack.line.expected_end) {
                                c->parser_stack.line.line_pos = c->parser_stack.line.expected_end = 0;
                                break;
                                //这里break到上面的读取数据，如果上面也没有数据就会回到外层
                            }
                            char temp_c = c->parser_stack.line.line_buffer[c->parser_stack.line.line_pos];
                            ++c->parser_stack.line.line_pos;
                            //* 读取长度，则字符只能是0 - 9的字符
                            if (temp_c >= '0' && temp_c <= '9') {
                                int16_t num = temp_c - '0';
                                c->parser_stack.bulk.expected = c->parser_stack.bulk.expected * 10 + num;
                                // 这里依次循环读取，直到读到\r
                                // 这个模式在LF中来设置
                            } else if (temp_c == '\r') {
                                c->parser_stack.array.current_state = STATE_BULK_READING_LENGTH_LF;
                                //当前字符是\r,设置读取\n的状态
                                // 然后break，再重新进入下面的\n
                                // 这里如果我\r读取到的是最后一个字符,那么初始化一下状态
                                // 因为上面的初始化是基于没有读到\r的
                                if (c->parser_stack.line.line_pos == c->parser_stack.line.expected_end) {
                                    c->parser_stack.line.line_pos = c->parser_stack.line.expected_end = 0;
                                    //重置然后break退出,从而进入到新的STATE_BULK_READING_LENGTH_LF
                                }
                                // 上面判断是否需要重置，重置与否都需要break
                                break;
                            } else {
                                c->is_back = false;
                                close(c->fd);
                                return -5;
                            }
                        }
                    } else if (c->parser_stack.array.current_state == STATE_BULK_READING_LENGTH_LF ||
                        c->parser_stack.array.current_state == STATE_BULK_READING_DATA_LF) {
                            
                        // 对于状态的转变, 上面在进入的时候就已经保证了读取数据了
                        // 上面的读取保证一定有数据, 这里去拿到line_pos位置的数据
                        char temp_c = c->parser_stack.line.lien_buffer[c->parser_stack.line.line_pos];
                        ++c->parser_stack.line.line_pos;
                        if (c->parser_stack.line.line_pos == c->parser_stack.line.expected_end) {
                            c->parser_stck.line.line_pos = c->parser_stack.line.expected_end = 0;
                        }
                        if (temp_c == '\n') {
                            // 说明当前读取到了\n
                            // 状态改变, 这里判断读取到的是哪个LF
                            // 
                            if (c->parser_stack.array.current_state == STATE_BULK_READING_LENGTH_LF) {
                                // 这里读取完批量字符串的长度，应该去读取数据
                                c->parser_stack.array.current_state = STATE_BULK_READING_DATA;
                                // 这里判断当前的长度是内联还是动态
                                if (c->parser_stack.bulk.expected > 127) {
                                    c->parser_stack.bulk.storage_type = 1;
                                    // 开辟空间,这里先用最简单的Malloc
                                    c->parser_stack.bulk.storage.heap_data = (char*)malloc(c->parser_stack.bulk.expected);
                                } else {
                                    c->parser_stack.bulk.storage_type = 0;
                                }
                                c->parser_stack.filled = 0;
                                break;
                            } else if (c->parser_stack.array.current_state == STATE_BULK_READING_DATA_LF) {
                                // 这里是数据读取读到了\n了
                                // 那么这里读取到了一个完整的数据了
                                // 那么肯定就是把当前的数据直接放到数组中，并且数组中的数据个数+1
                                // 如果数组中的数据个数达到了预期值，说明当前的数组的元素读取完
                                // 应该去读取下一个类型的数据.这里的类型应该是去到初始状态读取类型
                                // 下一个数据类型未知
                                // 这里把inline_data的数据放到inline_elements中
                                // 这里还需要判断是内联还是动态,因为访问的成员不同
                                // 如果还没有读取完数组元素，那么外层状态还是不变
                                // 这里是批量字符串数据读完了,相当于是一个类型读完了，必然需要添加到数组中
                                // 这类的批量字符串同时也需要
                                //? 先做一个大小判断
                                if (c->parser_stack.bulk.filled != c->parser_stack.bulk.expected) {
                                    c->is_back = true;
                                    close(c->fd);
                                    return -3;
                                }
                                // 大小相等，数据有效,存储到数组中，判断是内联还是动态
                                if (c->parser_stack.array.mode == MODE_INLINE_ONLY) {
                                    // 判断当前批量字符串是内联还是动态的,0内联,1内存池
                                    if (c->parser_stack.bulk.storage_type == 0) {
                                        //数据存储在内联数组中
                                        c->parser_stack.bulk.storage[c->parser_stack.bulk.filled] = '\0';
                                        // 保存到数组中,这里还需要先去内存池中申请内存
                                        block_alloc_t block = allocator_alloc(&global_allocator, c->parser_stack.bulk.filled + 1);
                                        memcpy(block->ptr, c->parser_stack.bulk.storage, c->parser_stack.bulk.filled + 1);
                                        c->parser_stack.array.inline_elements
                                    } else if (c->parser_stack.bulk.storage_type == 1) {
                                        c->parser_stack.bulk.storage.heap_data[c->parser_stack.bulk.filled] = '\0';
                                    }
                                } else if (c->parser_stack.array.mode == MODE_HYBRID) {

                                }
                                
                                // 继续判断是否需要继续读取元素
                                

                            }
                        } else {
                            // 不是\n，这里直接报错
                            c->is_back = true;
                            close(c->fd);
                            return -3;
                        }
                        
                    } else if (c->parser_stack.array.current_state == STATE_BULK_READING_DATA) {

                    } else if (c->parser_stack.array.current_state == STATE_BULK_READING_DATA_LF) {

                    }

                }

                

            } else if (c->parser_stack.frames.state == STATE_ARRAY_READING_ELEMENTS_INIT) {

                if (c->parser_stack.array.mode == MODE_INLINE_ONLY) {
                    // 使用静态内嵌数组，那么直接初始化一下就行
                    // 初始化期待元素个数
                    // 初始化当前的元素总数
                    // c->parser_stack.array.expected_count = c->parser_stack.array.inline_count;
                    // 初始化数组
                    memset(c->parser_stack.array.inline_elements, 0, sizeof(c->parser_stack.array.inline_elements));
                } else if (c->parser_stack.array.mode == MODE_HYBRID) {
                    //后续补充代码,这里先实现静态的，因为这里数组元素一般不会超过255
                }
                
                c->parser_stack.array.total_count = 0;
                c->parser_stack.frames.state = STATE_ARRAY_READING_ELEMENTS;
                c->parser_stack.array.current_state = STATE_READING_TYPE;
                c->parser_stack.array.current_type = FRAME_NONE;
                
            } else if (c->parser_stack.frames.state ==  STATE_BULK_READING_LENGTH) {

            } else if (c->parser_stack.frames.state ==  STATE_BULK_READING_DATA) {

            } else if (c->parser_stack.frames.state == STATE_COMPLETE) {

            } else if (c->parser_stack.frames.state == STATE_EXPECTING_ARRAY_LENGTH_LF) {

                // 一定要去读吗，可能line_buf中有数据还没有读完，还没有读完就不需要读取数据
                // 当line_buf中没有数据即line_pos == expected_end的时候才需要读取
                if (c->parser_stack.line.line_pos == c->parser_stack.line.expected_end) {

                    int read_byte = 0;
                    if (c->read_rb.Length >= 64) {
                        read_byte = 64;
                    } else if (c->read_rb.Length >= 32) {
                        read_byte = 32;
                    } else if (c->read_rb.Length >= 16) {
                        read_byte = 16;
                    } else if (c->read_rb.Length >= 8) {
                        read_byte = 8;
                    } else if (c->read_rb.Length >= 4) {
                        read_byte = 4;
                    } else if (c->read_rb.Length >= 2) {
                        read_byte = 2;
                    } else if (c->read_rb.Length >= 1) {
                        read_byte = 1;
                    } else {
                        return -3;
                    }
                    if (expected_end > MAX_LINE_BUFFER - read_byte) {
                        // 当前长度过长,直接断开连接
                        // 这里再设置个标记位上层循环先判断连接是否关闭如果关闭依次返回
                        // 返回到hook的recv然后直接退出该协程
                        c->is_back = true;
                        close(c->fd);
                        return;
                    }
                    if (RB_Read_String(&c->read_rb, c->parser_stack.line.line_buffer + c->parser_stack.line.line_pos, read_byte) == RING_BUFFER_ERROR) {
                        abort();
                        exit(-21);
                    }                   
                    // line_pos是当前解析的位置，expected_end是当前数据的末尾
                    expected_end += read_byte;
                }

                // 必然有数据,上面相等则读数据,不相等说明至少有1个字节
                char temp_c = c->parser_stack.line.line_buffer[c->parser_stack.line.line_pos];
                ++c->parser_stack.line.line_pos;
                if (c->parser_stack.line.line_pos == c->parser_stack.line.expected_end) {
                    c->parser_stack.line.line_pos = c->parser_stack.line.expected_end = 0;
                }
                if (temp_c == '\n') {
                    if (c->parser_stack.frames.state == STATE_EXPECTING_ARRAY_LENGTH_LF) {
                        // 读取完长度，下一个就是读取数组元素
                        // 说明当前长度有效,初始化当前总大小
                        c->parser_stack.
                        c->parser_stack.frames.state = STATE_ARRAY_READING_ELEMENTS_INIT;
                    }
                } else {
                    //期待\n但是却是其他字符直接关闭连接
                    c->is_back = true;
                    close(c->fd);
                    return -3;
                }
                // 去到下一个状态，初始化元素读取状态


                
            }
            
        }
        // 这里下面，没代码，这里弄完一个请求后状态标为解析完成一个请求，然后构造请求块给线程池
        // 每一个else if 出来后直接去到循环到达下一个位置
        
        
        
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

