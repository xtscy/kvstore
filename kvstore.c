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




// typedef struct kv_type_s kv_type_t;
// extern struct kv_type_s *g_kv_array;//* 等会编译的时候，两种情况都尝试一下
// fixed_size_pool_t *small_kv_pool = NULL;
// fixed_size_pool_t *medium_value_pool = NULL;
// fixed_size_pool_t *large_value_pool = NULL;
// extern fixed_size_pool_t *global_fixed_pool;
extern allocator_out_t global_allocator;
btree_handle global_bplus_tree;
int main(int argc, char* argv[]) {
    // size_t size = sizeof(kv_type_s);
    // fixed_pool_create()

    if (!allocator_out_init(&global_allocator)) {
        printf("failed init allocator\n");
    }
    global_fixed_pool = fixed_pool_create(sizeof(block_alloc_t), 100000);
    global_bplus_tree = btree_create_c(global_bplus_tree);

    Thread_Pool_Init(5);
    Thread_Pool_Run();   
    
    if (argc != 2) return -1;
    unsigned short port = atoi(argv[1]);
    printf("1\n");
    if(UseNet == NtyCo) {
        NtyCo_Entry(port);
    } else if (UseNet == Reactor){
        Reactor_Entry(port);
    }


    sleep(1000);
    return 0;
}