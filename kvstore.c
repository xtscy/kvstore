#include "kv_server.h"
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include "BPlusTree/bpt_c.h"
#include "Persister/persister_c.h"
#include "./MemoryBTreeLock/m_btree.h"
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

#define t 1024

extern _Atomic(uint16_t) fd_lock[20];
extern allocator_out_t global_allocator;
// btree_handle global_bplus_tree;
persister_handle global_persister;
extern btree_t *global_m_btree;

// 1 master 2slave
// ./kvstore task_port [1|2] m/s_port
volatile bool stage;
int main(int argc, char* argv[]) {
    // size_t size = sizeof(kv_type_s);
    // fixed_pool_create()

    if (!allocator_out_init(&global_allocator)) {
        printf("failed init allocator\n");
    }
    for (int i = 0; i < 20; i++) {
        atomic_init(&fd_lock[i], 0);
    }
    // printf("1\n");
    global_fixed_pool = fixed_pool_create(sizeof(block_alloc_t), 100);
    int_global_fixed_pool = fixed_pool_create(sizeof(int), 1000111);
    // printf("2\n");
    global_m_btree = btree_create(t);
    // global_bplus_tree = btree_create_c();
    // 
    #define full_path "fullLog.db"
    global_persister = persister_create_c(full_path);
    
    // printf("3\n");
    Thread_Pool_Init(1);
    Thread_Pool_Run();   
    // printf("4\n");
    if (argc != 4) return -1;
    unsigned short port = atoi(argv[1]);
    if (argv[2] == '1') {
        stage = true;
    } else if (argv[2] == '2') {
        stage = false;
    } else {
        printf("tips: ./kvstore task_port [1|2] m_port\n");
        return -1;
    }
    printf("5\n");
    if(UseNet == NtyCo) {
        // printf("6\n");
        NtyCo_Entry(port, atoi(argv[3]));
        // printf("7\n");
    } else if (UseNet == Reactor){
        Reactor_Entry(port);
    }


    printf("main运行完成, return 0 退出\n");
    // sleep(10000000);
    return 0;
}