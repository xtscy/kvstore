kvstore : hook_tcpserver.c kv_task.c kv_func.c kv_protocal.c kvstore.c kv_reactor.c ./RingBuffer/ring_buffer.c
	gcc $^ -o $@ -I ./NtyCo-master/core -L./NtyCo-master/ -lntyco
