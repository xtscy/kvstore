#include "kv_server.h"
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include "BPlusTree/bpt_c.h"

#if UseNet == Reactor
int kv_request(struct conn *c) {
    printf("recv %d: %s\n", c->rlength, c->rbuffer);
    printf("kv_request\n");
}
int kv_response(struct conn *c) {
    c->wlength = c->rlength;
	memcpy(c->wbuffer, c->rbuffer, c->wlength);
    printf("send %d: %s\n", c->wlength, c->wbuffer);
    printf("kv_response\n");
}
#endif

// typedef kv_type_s kv_type_t; 




extern allocator_out_t global_allocator;
btree_handle global_bplus_tree;
int main(int argc, char* argv[]) {
    // size_t size = sizeof(kv_type_s);
    // fixed_pool_create()

    if (!allocator_out_init(&global_allocator)) {
        printf("failed init allocator\n");
    }
    printf("1\n");
    global_fixed_pool = fixed_pool_create(sizeof(block_alloc_t), 100);
    printf("2\n");
    global_bplus_tree = btree_create_c();
    printf("3\n");
    Thread_Pool_Init(1);
    Thread_Pool_Run();   
    printf("4\n");
    if (argc != 2) return -1;
    unsigned short port = atoi(argv[1]);
    printf("5\n");
    if(UseNet == NtyCo) {
        printf("6\n");
        NtyCo_Entry(port);
        printf("7\n");
    } else if (UseNet == Reactor){
        Reactor_Entry(port);
    }


    printf("main运行完成, return 0 退出\n");
    // sleep(10000000);
    return 0;
}