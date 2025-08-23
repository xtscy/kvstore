#include "kv_server.h"
#include <stdio.h>
#include <stdlib.h>
#include "RingBuffer/ring_buffer.h"

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




int main(int argc, char* argv[]) {
    if (argc != 2) return -1;
    unsigned short port = atoi(argv[1]);
    printf("1\n");
    if(UseNet == NtyCo) {
        NtyCo_Entry(port);
    } else if (UseNet == Reactor){
        Reactor_Entry(port);
    }
    return 0;
}