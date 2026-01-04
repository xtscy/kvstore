#include "kv_task.h"
#include <string.h>
#include <poll.h>
#include "lock_free_ring_buf/lock_free_ring_buf.h"
#include <sys/socket.h>    // send() 函数声明
#include <sys/types.h>     // 数据类型定义（通常隐式包含）
#include "./NtyCo-master/core/nty_coroutine.h"
#include <stdatomic.h>
#include <arpa/inet.h>

char* order[] = {"set", "get", "del", "incr", "decr", "sort"};

extern int slave_array[10];
extern atomic_int slave_cnt;
extern volatile bool stage;

_Atomic(uint16_t) fd_lock[20];
// extern kv_type_t* g_kv_array;

ssize_t ssend(int fd, const void *buf, size_t len, int flags) {

	// if (!send_f) init_hook();

	// nty_schedule *sched = nty_coroutine_get_sched();
	// if (sched == NULL) {
		// return send_f(fd, buf, len, flags);
	// }

	int sent = 0;

	// int ret = send_f(fd, ((char*)buf)+sent, len-sent, flags);
	// if (ret == len) return ret;
	// if (ret > 0) sent += ret;

	while (sent < len) {
		// struct pollfd fds;
		// fds.fd = fd;
		// fds.events = POLLOUT | POLLERR | POLLHUP;

		// nty_poll_inner(&fds, 1, 1);
        printf("send_f behind\n");
		int ret = send_f(fd, ((char*)buf)+sent, len-sent, flags);
        printf("send_f after\n");
        if (ret == -1) {
            printf("send失败: %s\n", strerror(errno));
            abort();
        }
        printf("ret:%d\n", ret);

		//printf("send --> len : %d\n", ret);
		if (ret < 0) {			
            printf("当前send_f出错,ret=%d,continue\n", ret);
            abort();
            // continue;
            // exit(-11);
			// break;
		} else if (ret == 0) {
            printf("connect shutdown by peer\n");
            return ret;
        }
        if (ret >= 0) {
            sent += ret;
        }
	} 
    if (sent == len) return sent;
    else {
        printf("当前信息未发送完,eixt(-11)\n");
        sleep(1);
        printf("当前信息未发送完,eixt(-11)\n");
        abort();
        exit(-11);
    }
	// if (ret <= 0 && sent == 0) return ret;
	
	// return sent;
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
int Process_Data_Task(task_deli_t *task_block) {
    if (block == NULL) {
        perror("task_block null\n");
        abort();
    }

    if (task_block->mode == 1) {
        //set
        flag = KV_SET(task_block);
    } else if (task_block->mode == 2) {
        // get
    } else if (task_block->mode == 3) {
        //quit
    } else {
        return -1;
    }
    return 0;
}
/*int Process_Data_Task(block_alloc_t *block) {
    //* 拿到任务,解析任务，用if分支来判断执行哪个
    //* 这里不能使用strtok函数，为了保证线程安全，可重入函数
    //* 所以这里需要自己实现字符串分割,分割成一个一个token
    
    printf("96 block:->%s\n", block->ptr);
    char *token[2048] = {0};
    int num_token = 0;
    int prev = 0, pos = 0;
    int is_reading = -1;//-1 未读，1读
    int cur_token = 0;
    int del_cnt = 0;
    int size = block->size;
    //* 这里由于token个数有限，可以先解析一部分，执行任务然后再来分割，然后再来执行。循环，直到当前payload_len全部执行完毕。退出整个任务执行的循环
    while(size > 0) {
        
        is_reading = -1, del_cnt = 0, prev = 0;
        

        for (int i = pos; i < size; i++) {

            // if (num_token >= 64) {
            //     return -2;
            // }
            
            if (((char*)block->ptr)[pos] == ' ' || ((char*)block->ptr)[pos] == '\0') {
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
                case 0 : {
                    //* SET
                    flag = KV_SET(token[cur_token + 1], token[cur_token + 2]);

                    //* 通过返回flag的不同，从而发送不同的消息
                    //* 这里调用hook过的recv，发送消息并且可能让出
                    if (block->conn_fd != -10) {

                    
                        char buf[36] = {0};
                        if (flag == 0) {
                            // 发送OK
                            // buf = "OK";
                            sprintf(buf, "ok set %s %s", token[cur_token + 1], token[cur_token + 2]);
                            printf("set %s %s\n", token[cur_token + 1], token[cur_token + 2]);
                            printf("send:%s\n", buf);
                            
                            
                            if (stage == true) {
                                int cnt = atomic_load_explicit(&slave_cnt, memory_order_acquire);
                                if (cnt > 0) {
                                    printf("开始转发\n");
                                    // 这里可能多个线程都去send,这里使用了内核的socket锁
                                    // 实际上会串行
                                    char innbuf[64] = {0};
                                    int used = 0;
                                    used += snprintf(innbuf + sizeof(int), sizeof(innbuf) - sizeof(int), "set %s %s", token[cur_token + 1], token[cur_token + 2]);
                                    if (used >= sizeof(innbuf) - sizeof(int)) {
                                        perror("发送的消息过长,exit退出\n");
                                        printf("!!!!!!!!!!!!!!!!!!!!!\n");
                                        printf("!!!!!!!!!!!!!!!!!!!!!\n");
                                        printf("!!!!!!!!!!!!!!!!!!!!!\n");
                                        abort();
                                        exit(-13);
                                    }
                                    uint32_t tmp_len = htonl(strlen(innbuf + 4));
                                    memcpy(innbuf, &tmp_len, sizeof(tmp_len));
                                    
                                    for (int i = 0; i < cnt; i++) {
                                        ssend(slave_array[i], innbuf, strlen(innbuf + 4) + 4, 0);
                                        printf("转发%s\n", innbuf + 4);
                                    }
                                    printf("转发完成\n");
                                }
                            }

                        } else if (flag == -1) {
                            sprintf(buf, "%s", "FALSE");
                            // buf = "FALSE";
                            abort();
                        }
                        uint16_t val = 0;
                        ssize_t sign = 0;
                        do {
                            val = atomic_load_explicit(&fd_lock[block->conn_fd], memory_order_acquire);
                            
                            if (val & 1) {
                                continue;
                            }
                            if (atomic_compare_exchange_weak_explicit(&fd_lock[block->conn_fd], &val, val + 1, memory_order_acquire, memory_order_relaxed)) {
                                // ssend(block->conn_fd, s_buf, strlen(s_buf), 0);
                                sign = ssend(block->conn_fd, buf, strlen(buf), 0);
                                if (sign == -1 || sign == 0) {
                                    atomic_fetch_add_explicit(&fd_lock[block->conn_fd], 1, memory_order_release);
                                    return -3;
                                }
                                // atomic_store_explicit(&fd_lock[block->conn_fd], false, memory_order_release);
                                atomic_fetch_add_explicit(&fd_lock[block->conn_fd], 1, memory_order_release);
                                break;
                            }
                                    
                        } while(true);
                    }
                    cur_token += 3;
                    break;
                }
                case 1 : {
                    //* GET
                    printf("GET request\n");
                    int pos = 0;

                    flag = KV_GET(token[cur_token + 1], &pos);
                    if (block->conn_fd != -10) {
                        char buf[16] = {0};
                        if (flag == 0) {
                            //* 存在，先把值转换成字符串
                            //* 通过类型判断，如果是字符串直接发送，如果是数值类型则转换
                            // if (g_kv_array[pos].type == TYPE_INTEGER) {
                                // char s_buf[16] = {0};//解引用,就是把拿到指针指向的对象,int a = 10 int *pt = &a , *pt = a = 10,
                                sprintf(buf, "%d\r\n", pos);
                                
                                printf("send:%s\n", buf);
                            // } else if (g_kv_array[pos].type == TYPE_STRING) {
                                // ssend(t->conn_fd, g_kv_array[pos].value, strlen(g_kv_array[pos].value), 0);
                            // }
                            
                        } else if (flag == -1) {
                            // buf = "FALSE";
                            sprintf(buf, "%s", "FALSE\r\n");
                            // ssend(block->conn_fd, buf, strlen(buf), 0);
                            //* send 不存在
                        }
                        ssize_t sign = 0;
                        uint16_t val = 0;
                        do {
                            val = atomic_load_explicit(&fd_lock[block->conn_fd], memory_order_acquire);
                            
                            if (val & 1) {
                                // printf("val & 1 continue");
                                continue;
                            }
                            if (atomic_compare_exchange_weak_explicit(&fd_lock[block->conn_fd], &val, val + 1, memory_order_acquire, memory_order_relaxed)) {
                                // ssend(block->conn_fd, s_buf, strlen(s_buf), 0);
                                sign = ssend(block->conn_fd, buf, strlen(buf), 0);
                                if (sign == -1 || sign == 0) {
                                    atomic_fetch_add_explicit(&fd_lock[block->conn_fd], 1, memory_order_release);
                                    abort();
                                }
                                // atomic_store_explicit(&fd_lock[block->conn_fd], false, memory_order_release);
                                atomic_fetch_add_explicit(&fd_lock[block->conn_fd], 1, memory_order_release);
                                break;
                            }
                                    
                        } while(true);
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
                case 5 : {
                    //todo SORT

                    char *temp_send_array = NULL;
                    // true send, false false
                    flag = KV_SORT(token[cur_token + 1], &temp_send_array);
                    if (temp_send_array == NULL) {
                        printf("KV_SORT temp_send_array NULL\n");
                        abort();
                    }
                    if (block->conn_fd != -10) {
                        if (flag == -1) {
                            sprintf(temp_send_array, "%s", "FALSE");
                        } else if (flag == -2) {
                            sprintf(temp_send_array, "%s", "not exsist value");
                        } else {
                            ssize_t sign = 0;
                            uint16_t val = 0;
                            do {
                                val = atomic_load_explicit(&fd_lock[block->conn_fd], memory_order_acquire);
                                
                                if (val & 1) {
                                    printf("val & 1 continue");
                                    continue;
                                }
                                if (atomic_compare_exchange_weak_explicit(&fd_lock[block->conn_fd], &val, val + 1, memory_order_acquire, memory_order_relaxed)) {
                                    // ssend(block->conn_fd, s_buf, strlen(s_buf), 0);
                                    sign = ssend(block->conn_fd, temp_send_array, strlen(temp_send_array), 0);
                                    if (sign == -1 || sign == 0) {
                                        atomic_fetch_add_explicit(&fd_lock[block->conn_fd], 1, memory_order_release);
                                        abort();
                                    }
                                    // atomic_store_explicit(&fd_lock[block->conn_fd], false, memory_order_release);
                                    atomic_fetch_add_explicit(&fd_lock[block->conn_fd], 1, memory_order_release);
                                    break;
                                }
                                        
                            } while(true);
                        }
                    }
                    free(temp_send_array);
                    cur_token += 2;
                    break;
                }
            }
            // cur_token++;
        }

        num_token = 0;
        size -= del_cnt;
    }
    printf("int Process_Data_Task(block_alloc_t *block) 正常返回\n");
    return 0;//* 0成功
    
}   
*/
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