# kvstore : hook_tcpserver.c kv_task.c kv_func.c kv_protocal.c kvstore.c kv_reactor.c ./RingBuffer/ring_buffer.c ./threadpool/threadpool.c ./lock_free_ring_buf/lock_free_ring_buf.c
# 	gcc $^ -o $@ -I ./NtyCo-master/core -L./NtyCo-master/ -lntyco
kvstore : hook_tcpserver.c kv_task.c kv_func.c kv_protocal.c kvstore.c kv_reactor.c ./RingBuffer/ring_buffer.c ./threadpool/threadpool.c ./lock_free_ring_buf/lock_free_ring_task_buf.c
	gcc-13 $^ -o $@ -I ./NtyCo-master/core -L./NtyCo-master/ -lntyco -std=c17 \