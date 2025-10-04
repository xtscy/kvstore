#include "kv_server.h"
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>

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
extern struct kv_type_s *g_kv_array;//* 等会编译的时候，两种情况都尝试一下
fixed_size_pool_t *small_kv_pool = NULL;
// fixed_size_pool_t *medium_value_pool = NULL;
// fixed_size_pool_t *large_value_pool = NULL;

int main(int argc, char* argv[]) {
    // size_t size = sizeof(kv_type_s);
    // fixed_pool_create()



    // if (argc != 2) return -1;
    // unsigned short port = atoi(argv[1]);
    // printf("1\n");
    // if(UseNet == NtyCo) {
    //     NtyCo_Entry(port);
    // } else if (UseNet == Reactor){
    //     Reactor_Entry(port);
    // }

    Thread_Pool_Init(5);
    Thread_Pool_Run();

    sleep(1000);
    return 0;
}