
#include "./NtyCo-master/core/nty_coroutine.h"

#include <arpa/inet.h>

#include "kv_protocal.h"
#include <sys/epoll.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>

#define MAX_EVENTS 2

extern _Atomic(uint16_t) fd_lock[20];
void server_reader(void *arg) {
	int fd = *(int *)arg;
	// uint32_t read_byte = 0;
	int ret = 0;
//* 在这里初始化环形缓冲区,并且用一个结构体来描述当前连接,这是为了解决粘包和分包问题
//* 编译时初始化，性能更好
	connection_t c = {0};
	
	c.fd = fd;
	RB_Init(&c.read_rb, RING_BUF_SIZE);
	int epoll_fd = epoll_create1(0);
	if (epoll_fd < 0) {
        perror("epoll_create1");
        // close(server_fd);
		exit(-12);
        // return 1;
    }
	struct epoll_event ev = {0};
    
    ev.events = EPOLLIN;
    ev.data.fd = fd;
    
    if (epoll_ctl(epoll_fd, EPOLL_CTL_ADD, fd, &ev) < 0) {
        perror("epoll_ctl ADD");
		exit(-6);
        // return 
    }
	struct epoll_event events[MAX_EVENTS];
	int nfds = 0;
	while (1) {
		// char buf[1024] = {0};
		// ret = recv(fd, buf, 1024, 0);
		// read_byte = RB_Get_FreeSize(&c.read_rb) < READ_CACHE_SIZE ? RB_Get_FreeSize(&c.read_rb) : READ_CACHE_SIZE;
		//* 这里尽可能多的读到环形缓冲区，然后在依次放入环形缓冲区处理消息
		//* 可以用一个循环，如果缓冲区读完了，那就继续从临时缓冲区拿。缓冲区读完则跳出处理，来到外层继续下一个循环。
		// *这里可能该read_cache中也存在拆包所以这里还需要判断read_cache
		printf("wait recv\n");
		nfds = epoll_wait(epoll_fd, events, MAX_EVENTS, -1);
		if (nfds < 0) {
            perror("epoll_wait failed \n");
			exit(-5);
            // break;
        }
		// 这里如果读到READ_CACHE_SIZE,最后一个请求的数据在下一个包里面，这里就是一个拆包的处理
		// if (c.read_cache.head != c.read_cache.length) {

		// }
		// if (nfds != 1 || events[nfds - 1].data.fd != fd) {
		// 	perror("epoll_wait failed\n");
		// 	exit(-5);
		// }
		uint16_t val = 0;
		do {
			val = atomic_load_explicit(&fd_lock[fd], memory_order_acquire);
			
			if (val & 1) {
				continue;
			}
			if (atomic_compare_exchange_weak_explicit(&fd_lock[fd], &val, val + 1, memory_order_release, memory_order_relaxed)) {
				ret = recv(fd, c.read_cache.cache, READ_CACHE_SIZE, 0);
				atomic_fetch_add_explicit(&fd_lock[fd], 1, memory_order_release);
				break;
			}
		} while(true);
		
		// ret = recv(fd, c.read_cache.cache, READ_CACHE_SIZE, 0);
		if (ret > 0) {
			printf("ret:%d\n",ret);
			printf("c->read_rb.Length:%u\n",c.read_rb.Length);
			c.read_cache.length = ret;
			c.read_cache.head = 0;
			Process_Message(&c);//* 处理一个完整的请求后发送数据
			//* 这里可以优化成处理完全部数据后，再调用1次send来发送所有数据
			// printf("read from server: %.*s\n", ret, buf);
			// #if UseNet == NtyCo
			// 	char resp[1024] = {0};
			// 	// ret = kv_protocal(buf, ret, resp); 
			// ret = send(fd, resp, ret, 0);
			// #else
			// 	printf("read from server: %.*s\n", ret, buf);
			// 	ret = send(fd, buf, strlen(buf), 0);
			// #endif
			
		} else if (ret == 0) {	
			close(fd);
			
			break;
		} else  {
			close(fd);
			printf("recv failed\n");
			exit(-7);
			// break;
		}

	}
}



void server(void *arg) {
	unsigned short port = *(unsigned short *)arg;
	int fd = socket(AF_INET, SOCK_STREAM, 0);
	if (fd < 0) return ;

	struct sockaddr_in local, remote;
	local.sin_family = AF_INET;
	local.sin_port = htons(port);
	local.sin_addr.s_addr = INADDR_ANY;
	bind(fd, (struct sockaddr*)&local, sizeof(struct sockaddr_in));
	printf("listen before\n");
	listen(fd, 20);
	printf("listen after\n");
	printf("listen port : %d\n", port);

	while (!0) {
		
		socklen_t len = sizeof(struct sockaddr_in);
		int cli_fd = accept(fd, (struct sockaddr*)&remote, &len);
		nty_coroutine *read_co;
		nty_coroutine_create(&read_co, server_reader, &cli_fd);

	}
	
}


// kv_type_t* g_kv_array = NULL;
// extern fixed_size_pool_t *small_kv_pool;
// extern fixed_size_pool_t *medium_value_pool;
// extern fixed_size_pool_t *large_value_pool;

// 开辟多个小块因为指令随机到来，

// 

int NtyCo_Entry(unsigned short p) {
	size_t size = sizeof(kv_type_t);

	// small_kv_pool = fixed_pool_create(SMALL_SIZE, 3000000);
	// small_value_pool = fixed_pool_create(MEDIUM_SIZE, 3000000);
	// 当存储值时，才去alloc分配块，并且alloc时把

	// g_kv_array = (kv_type_t*)malloc(sizeof(kv_type_t) * KV_ARRAY_SIZE);
	printf("8\n");
	int port = p;
	
	nty_coroutine *co = NULL;
	nty_coroutine_create(&co, server, &port);
	printf("9\n");
	nty_schedule_run();
	printf("10\n");
	return 0;
}




