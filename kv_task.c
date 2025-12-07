#include "kv_task.h"
#include <string.h>
#include <poll.h>
#include "lock_free_ring_buf/lock_free_ring_buf.h"
#include <sys/socket.h>    // send() 函数声明
#include <sys/types.h>     // 数据类型定义（通常隐式包含）

char* order[] = {"set", "get", "del", "incr", "decr"};


extern kv_type_t* g_kv_array;

ssize_t ssend(int fd, const void *buf, size_t len, int flags) {

	// if (!send_f) init_hook();

	// nty_schedule *sched = nty_coroutine_get_sched();
	// if (sched == NULL) {
		// return send_f(fd, buf, len, flags);
	// }

	int sent = 0;

	int ret = send(fd, ((char*)buf)+sent, len-sent, flags);
	if (ret == 0) return ret;
	if (ret > 0) sent += ret;

	while (sent < len) {
		// struct pollfd fds;
		// fds.fd = fd;
		// fds.events = POLLOUT | POLLERR | POLLHUP;

		// nty_poll_inner(&fds, 1, 1);
		ret = send(fd, ((char*)buf)+sent, len-sent, flags);
		//printf("send --> len : %d\n", ret);
		if (ret <= 0) {			
            exit(-11);
			break;
		} else if (ret == 0) {
            printf("connect shutdown by peer\n");
            return ret;
        }
		sent += ret;
	}

	if (ret <= 0 && sent == 0) return ret;
	
	return sent;
}
// extern ssize_t send(int fd, const void *buf, size_t len, int flags);

//? 返回值为解析到的token个数
// int split_data(connection_t *c) {
//     //* 从buf中获取到token
//     char *buf = c->pkt_data;
//     int count = 0;
//     char *token = strtok(buf, " ");
//     while (token != NULL) {//int a , &a =pa, (*pa) = a, (*pa).fd = pa->fd 
//         c->tokens[count++] = token; //(*c).token[]
//         token = strtok(NULL, " ");
//     }
//     return count;
// }                                                                                                                                                                       

int Process_Data_Task(block_alloc_t *block) {
    //* 拿到任务,解析任务，用if分支来判断执行哪个
    //* 这里不能使用strtok函数，为了保证线程安全，可重入函数
    //* 所以这里需要自己实现字符串分割,分割成一个一个token
    

    char *token[64] = {0};
    int num_token = 0;
    int prev = 0, pos = 0;
    int is_reading = -1;//-1 未读，1读
    int cur_token = 0;
    int del_cnt = 0;
    //* 这里由于token个数有限，可以先解析一部分，执行任务然后再来分割，然后再来执行。循环，直到当前payload_len全部执行完毕。退出整个任务执行的循环
    while(block->size > 0) {    
        
        is_reading = -1, del_cnt = 0, prev = 0;
        

        for (int i = pos; i < block->size; i++) {

            // if (num_token >= 64) {
            //     return -2;
            // }
            
            if (block->ptr[pos] == ' ' || block->ptr[pos] == '\0') {
                if (is_reading == -1) {
                    //未读
                    pos++;
                    del_cnt++;
                } else if (is_reading == 1) {
                    //* 正在读，遇到空格，直接结束
                    token[num_token++] = &block->ptr[prev];

                    is_reading = -1;
                    block->ptr[pos] = 0;
                    pos++;
                    del_cnt++;
                    if (num_token >= 64) {
                        //* 退出当前循环，直接去执行任务，执行完，再重置num_token
                        break;
                    }
                }
            } else if (
            (((int)(block->ptr[pos])) >= 'A' && block->ptr[pos] <= 'Z') || 
            (((int)(block->ptr[pos])) >= 'a' && ((int)(block->ptr[pos])) <= 'z') || 
            (((int)(block->ptr[pos])) >= '0' && ((int)(block->ptr[pos])) <= '9')
            ) {
                if (is_reading == -1) {
                    prev = pos;
                    is_reading = 1;
                }
                pos++;
                del_cnt++;
                continue;
            }

        }

        cur_token = 0;


        while (cur_token < num_token) {


            int ret = token_to_order(token[cur_token]);
            if (ret == -1) {
                return -2;//* 这里可以遇到错误直接返回，也可以跳过当前的token
            }
            int flag = 0;
            switch (ret) {
                // 更改架构使用内存池，所以这里的KV_函数都需要更改
                case 0 : {
                    //* SET
                    // 这里使用disk_bPlusTree
                    flag = KV_SET(token[cur_token + 1], token[cur_token + 2]);
                    //* 通过返回flag的不同，从而发送不同的消息
                    //* 这里调用hook过的recv，发送消息并且可能让出
                    if (flag == 0) {
                        //* 发送OK
                        char* buf = "OK";
                        ssend(block->conn_fd, buf, strlen(buf), 0);
                        printf("send:%s\n", buf);
                    } else if (flag == -1) {
                        char *buf = "FALSE";
                        ssend(block->conn_fd, buf, strlen(buf), 0);
                    }
                    cur_token += 3;
                    break;
                } 
                case 1 : {
                    //* GET
                    printf("GET request\n");
                    int pos = 0;
                    flag = KV_GET(token[cur_token + 1], &pos);
        
                    if (flag == 0) {
                        //* 存在，先把值转换成字符串
                        //* 通过类型判断，如果是字符串直接发送，如果是数值类型则转换
                        // if (g_kv_array[pos].type == TYPE_INTEGER) {
                            char s_buf[16] = {0};//解引用,就是把拿到指针指向的对象,int a = 10 int *pt = &a , *pt = a = 10,
                            sprintf(s_buf, "%d", pos);
                            ssend(block->conn_fd, s_buf, strlen(s_buf), 0);
                            printf("ssend:%s\n", s_buf);
                        // } else if (g_kv_array[pos].type == TYPE_STRING) {
                            // ssend(t->conn_fd, g_kv_array[pos].value, strlen(g_kv_array[pos].value), 0);
                        // }
                        
                    } else if (flag == -1) {
                        char *buf = "FALSE";
                        ssend(block->conn_fd, buf, strlen(buf), 0);
                        //* send 不存在
                    }
                    cur_token += 2;
                    break;
                }
                case 2 : {
                    //* DEL
                    flag = KV_DEL(token[cur_token + 1]);
                    break;
                }
                case 3 : {
                    //* INCR
                    flag = KV_INCR(token[cur_token + 1]);
                    break;
                }
                case 4 : {
                    //* DECR
                    flag = KV_DECR(token[cur_token + 1]);
                    break;
                }
            }
            // cur_token++;
        }

        num_token = 0;
        block->size -= del_cnt;
    }

    return 0;//* 0成功
    

}   

int token_to_order(char *buf) {
    //* 遍历数组，找到对应的
    for (unsigned long i = 0; i < sizeof(order)/sizeof(order[0]); i++) {
        if (0 == strcmp(buf, order[i])) {
            return i;
        }
    }
    return -1;
}

//* 这个函数可以不需要了。上层IO读到BODY时，通过调度器选择线程，然后拷贝BODY到一个存储区，然后让队列指针指向这个task
/*
int Process_Task(connection_t *c) {
    //* 直接从pkt_data处理数据，从current_header知道当前数据长度
    int count = split_data(c);
    if(count == 0) {
        // poll(NULL, NULL, 1000);
        printf("当前pkt_data不存在token\n");
        return -1;
    }//SET KEY VALUE
    int ret = token_to_order(c->tokens[0]);
    if (ret == -1) {
        printf("当前token没有对应的order\n");
        return -2;
    }
    int flag = 0;
    switch (ret) {
        case 0 : {
            //* SET
            flag = KV_SET(c, c->tokens[1], c->tokens[2]);
            //* 通过返回flag的不同，从而发送不同的消息
            //* 这里调用hook过的recv，发送消息并且可能让出
            if (flag == 0) {
                //* 发送OK
                char* buf = "OK";
                send(c->fd, buf, strlen(buf), 0);
                printf("send:%s\n", buf);
            } else if (flag == -1) {
                char *buf = "FALSE";
                send(c->fd, buf, strlen(buf), 0);
            }
            break;
        } 
        case 1 : {
            //* GET
            printf("GET request\n");
            int pos = -1;
            flag = KV_GET(c, c->tokens[1], &pos);

            if (flag == 0) {
                //* 存在，先把值转换成字符串
                //* 通过类型判断，如果是字符串直接发送，如果是数值类型则转换
                if (c->kv_array[pos].type == TYPE_INTEGER) {
                    char s_buf[16] = {0};//解引用,就是把拿到指针指向的对象,int a = 10 int *pt = &a , *pt = a = 10,
                    sprintf(s_buf, "%ld", *(long*)c->kv_array[pos].value);
                    send(c->fd, s_buf, strlen(s_buf), 0);
                    printf("send:%s\n", s_buf);
                } else if (c->kv_array[pos].type == TYPE_STRING) {
                    send(c->fd, c->kv_array[pos].value, strlen(c->kv_array[pos].value), 0);
                }

            } else if (flag == -1) {
                char *buf = "FALSE";
                send(c->fd, buf, strlen(buf), 0);
                //* send 不存在
            }
            break;
        }
        case 2 : {
            //* DEL
            flag = KV_DEL(c, c->tokens[1]);
            break;
        }
        case 3 : {
            //* INCR
            flag = KV_INCR(c, c->tokens[1]);
            break;
        }
        case 4 : {
            //* DECR
            flag = KV_DECR(c, c->tokens[1]);
            break;
        }
    }
    return 0;
}
*/