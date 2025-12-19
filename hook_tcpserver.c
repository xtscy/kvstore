
#include "./NtyCo-master/core/nty_coroutine.h"

#include <arpa/inet.h>

#include "kv_protocal.h"
#include <sys/epoll.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>

#define MAX_EVENTS 2


typedef struct process_data_s {

	int current_fd;
	in_addr_t s_addr;
	
} process_data_t;


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

// *这里需要两个函数，一个是主服务器运行的接收从机连接的函数， 
//* 一个是从机要执行连接主机的函数，连接主机，主机接收连接，然后从机发送自己的任务处理端口和自己的ip
//* 主机接收到ip和端口号，然后主机给从机发送连接请求，然后从机接收，然后建立连接
//* 然后主机发送自己内存中的数据给从机,按照协议格式进行发送，从机的任务线程进行数据的同步
//* 然后主机接收操作时，同时把这个操作也发给从机，从机的任务线程执行该操作，把数据写到内存中
//* 这里从机不用写磁盘，也不用读磁盘，同步来自于主机，没有增量日志和全量日志

//todo 主机需要实现连接的接收并接收网络信息，当从机断开连接后
//todo 回连从机的任务处理服务用接收的ip和端口号,然后发送当前内存中的数据按照协议格式，并在接收操作时转发操作
//todo 从机需要实现主动连接主机，并且发送自己的ip和任务端口号，然后即可断开连接


void process_slave(void *arg) {


	
	
	
}


void process_master(void *arg) {
	// 这里已经接收到了一个连接然后去接收数据,协议格式以\r\n结尾
	// 实现接收从机请求并接收数据的功能
	// 这里就一个一个的处理吗，先这样，后续可以批处理
	// 给一个时间区间，同一个区间的一起处理
	process_data_t *pd = (process_data_t*)arg;
	char buffer[1024] = {0};
	ssize_t bytes_received = 0;
	ssize_t total = 0;
	while (true) {
		bytes_received = recv(pd->current_fd, buffer + total, sizeof(buffer) - 1, 0);
		total += bytes_received;
		// 作协议解析
		// 如果不以\r\n结尾, 那么继续循环读，直到判断以\r\n结尾，那么接收了完整的数据，然后退出循环
		if (buffer[total - 2] == '\r' 
		&& buffer[total - 1] == '\n') 
		{
			break;
		}
	}

	// 读到完整请求,处理字符串
	// 127.0.0.1,这里不需要ip,只需要从机的任务处理服务的端口号
	in_port_t port = atoi(buffer);
	// 拿到了port和ip信息
	// 连接从机的任务服务
	// 先关闭当前的连接,对端读到0,表示对端知道消息接收成功，然后退出连接任务函数
	// 由当前线程去连接任务服务端口
	close(pd->current_fd);
	// 连接对端，需要对端的ip和端口并且都需要是网络序
	struct sockaddr_in slave_addr = {0};
	memset(&slave_addr, 0, sizeof(slave_addr));
	slave_addr.sin_family = AF_INET;
	slave_addr.sin_port = htons(port);
	slave_addr.sin_addr.s_addr = pd->s_addr;

	int slave_fd = socket(AF_INET, SOCK_STREAM, 0);
	if (slave_fd < 0) {
        perror("socket creation failed");
		printf("!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!\n");
		printf("!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!\n");
		printf("!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!\n");
        return -1;
    }
	
	int ret = connect(slave_fd, (struct sockaddr*)&slave_addr, sizeof(slave_addr));
	if (ret < 0) {
		perror("connection failed");
		printf("!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!\n");
		printf("!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!\n");
		printf("!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!\n");
		printf("!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!\n");
        close(slave_fd);
        return -1;
	}

	// 连接成功，那么遍历内存中的数据
	// 组织成正确的协议格式发送给对端
	// 这里的内存数据结构是b树，那么如何遍历呢，还是需要一个类似的迭代器
	// 去指向每一个数据
	
}
// 
// 主机运行该函数
void replicate(void *arg) {
	// 接收从机请求，以及接收从机的ip和端口
	// 再向从机的任务端口发送连接
	unsigned short port = *(unsigned short*)arg;
	//listen recv
	int fd = socket(AF_INET, SOCK_STREAM, 0);
	if (fd < 0) return ;

	struct sockaddr_in local, remote;
	local.sin_family = AF_INET;
	local.sin_port = htons(port);
	local.sin_addr.s_addr = INADDR_ANY;
	bind(fd, (struct sockaddr*)&local, sizeof(struct sockaddr_in));
	listen(fd, 20);
	// 监听，然后接收连接，然后去执行同步任务
	printf("listen master port : %d\n", port);

	while (!0) {
		socklen_t len = sizeof(struct sockaddr_in);
		int cli_fd = accept(fd, (struct sockaddr*)&remote, &len);
		nty_coroutine *pro_mst;
		process_data_t *pd = (process_data_t*)malloc(sizeof(process_data_t));
		pd->current_fd = cli_fd;
		pd->s_addr = remote.sin_addr.s_addr;
		nty_coroutine_create(&pro_mst, process_master, pd);
	}
}


extern volatile bool stage;
int NtyCo_Entry(unsigned short p, unsigned short master) {
	size_t size = sizeof(kv_type_t);

	// small_kv_pool = fixed_pool_create(SMALL_SIZE, 3000000);
	// small_value_pool = fixed_pool_create(MEDIUM_SIZE, 3000000);
	// 当存储值时，才去alloc分配块，并且alloc时把

	// g_kv_array = (kv_type_t*)malloc(sizeof(kv_type_t) * KV_ARRAY_SIZE);
	printf("8\n");
	unsigned short port = p;
	unsigned short m_port = master;
	nty_coroutine *co = NULL;
	nty_coroutine_create(&co, server, &port);
	nty_coroutine *m_co = NULL;
	if (stage == true) {
		nty_coroutine_create(&m_co, replicate, &m_port);
	}
	printf("9\n");
	nty_schedule_run();
	printf("10\n");
	return 0;
}




