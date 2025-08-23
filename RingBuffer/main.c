#include "ring_buffer.h"
#include <stdio.h>
#include <string.h>

int main() {
    ring_buffer rb;
    RB_Init(&rb, 128);
    // char c[] = {'h', 'e', 'l', 'l', 'o'};
    char c[50] = {0};
    sprintf(c, "hello ringbuf!");
    RB_Write_String(&rb, c, strlen(c));
    printf("length: %d, Freesize:%d\n", RB_Get_Length(&rb), RB_Get_FreeSize(&rb));
    char rbuf[100] = {0};
    RB_Read_String(&rb, rbuf, RB_Get_Length(&rb) - 1);
    printf("read:%s\n", rbuf);
    return 0;
}


