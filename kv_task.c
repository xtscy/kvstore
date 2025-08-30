#include "kv_task.h"
#include <string.h>
#include <poll.h>

char* order[] = {"SET", "GET", "DEL", "INCR", "DECR"};
s
extern ssize_t send(int fd, const void *buf, size_t len, int flags);

//? 返回值为解析到的token个数
int split_data(connection_t *c) {
    //* 从buf中获取到token
    char *buf = c->pkt_data;
    int count = 0;
    char *token = strtok(buf, " ");
    while (token != NULL) {//int a , &a =pa, (*pa) = a, (*pa).fd = pa->fd 
        c->tokens[count++] = token; //(*c).token[]
        token = strtok(NULL, " ");
    }
    return count;
}                                                                                                                                                                       

int token_to_order(char *buf) {
    //* 遍历数组，找到对应的
    for (int i = 0; i < sizeof(order)/sizeof(order[0]); i++) {
        if (0 == strcmp(buf, order[i])) {
            return i;
        }
    }
    return -1;
}


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