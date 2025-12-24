
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

typedef struct ports_s {
	unsigned short m_port;
	unsigned short task_port;
} ports_t;

// 主线程负责发送，所以只需要存储值即可
// 这里还需要考虑到从机如果断开连接，那么这里应该清除对应的fd
// 这里可以使用最简单的send返回值判断，而非recv得到返回值0
// typedef struct slave_fd_s {
// 	int fd;
// 	bool true;
// } slave_fd_t;

int slave_array[10];
atomic_int slave_cnt;
typedef struct fds_s {
	int peer_fd;
	int c_fd;
} fds_t;
extern _Atomic uint16_t fd_lock[20];
void server_reader(void *arg) {
	fds_t fd = *(fds_t *)arg;
	free(arg);
	// uint32_t read_byte = 0;
	int ret = 0;
// 在这里初始化环形缓冲区,并且用一个结构体来描述当前连接,解决粘包和分包问题

	connection_t *c = (connection_t*)malloc(sizeof(connection_t));
	memset(&c->read_cache, 0, sizeof(read_cache_t));
	c->state = PARSE_STATE_HEADER;
	c->current_header = 0;
	c->fd = fd.c_fd;
	RB_Init(&c->read_rb, RING_BUF_SIZE);
	/* 
	// int epoll_fd = epoll_create1(0);
	// if (epoll_fd < 0) {
    //     perror("epoll_create1");
    //     // close(server_fd);
	// 	exit(-12);
    //     // return 1;
    // }
	// struct epoll_event ev = {0};
    
    // ev.events = EPOLLIN;
    // ev.data.fd = fd;
    
    // if (epoll_ctl(epoll_fd, EPOLL_CTL_ADD, fd, &ev) < 0) {
        // perror("epoll_ctl ADD");
		// exit(-6);
        // return 
    // }
	// struct epoll_event events[MAX_EVENTS];
	// int nfds = 0;
	*/
	while (1) {
		// char buf[1024] = {0};
		// ret = recv(fd, buf, 1024, 0);
		// read_byte = RB_Get_FreeSize(&c.read_rb) < READ_CACHE_SIZE ? RB_Get_FreeSize(&c.read_rb) : READ_CACHE_SIZE;
		//* 这里尽可能多的读到环形缓冲区，然后在依次放入环形缓冲区处理消息
		//* 可以用一个循环，如果缓冲区读完了，那就继续从临时缓冲区拿。缓冲区读完则跳出处理，来到外层继续下一个循环。
		// *这里可能该read_cache中也存在拆包所以这里还需要判断read_cache
		// printf("wait recv\n");
		// nfds = epoll_wait(epoll_fd, events, MAX_EVENTS, -1);
		// if (nfds < 0) {
            // perror("epoll_wait failed \n");
			// exit(-5);
            // break;
        // }
		// 这里如果读到READ_CACHE_SIZE,最后一个请求的数据在下一个包里面，这里就是一个拆包的处理
		// if (c.read_cache.head != c.read_cache.length) {

		// }
		// if (nfds != 1 || events[nfds - 1].data.fd != fd) {
		// 	perror("epoll_wait failed\n");
		// 	exit(-5);
		// }
		// 这里socket有内核锁，也可以直接去recv
		// 多个线程最后都会变成串行化
		uint16_t val = 0;
		// do {
		// 	val = atomic_load_explicit(&fd_lock[fd], memory_order_acquire);
			
		// 	if (val & 1) {
		// 		continue;
		// 	}
		// 	if (atomic_compare_exchange_weak_explicit(&fd_lock[fd], &val, val + 1, memory_order_release, memory_order_relaxed)) {
		// 		ret = recv(fd, c->read_cache.cache, READ_CACHE_SIZE, 0);
		// 		atomic_fetch_add_explicit(&fd_lock[fd], 1, memory_order_release);
		// 		break;
		// 	}
		// } while(true);
		c->read_cache.length = 0;
		c->read_cache.head = 0;
		ret = nty_recv(fd.peer_fd, c->read_cache.cache, READ_CACHE_SIZE, 0);
		
		// ret = recv(fd, c.read_cache.cache, READ_CACHE_SIZE, 0);
		if (ret > 0) {
			printf("ret:%d\n",ret);
			printf("c->read_rb.Length:%u\n",c->read_rb.Length);
			c->read_cache.length = ret;
			c->read_cache.head = 0;
			Process_Message(c);//* 处理一个完整的请求后发送数据
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
			close(fd.peer_fd);
			return;
		} else {
			close(fd.peer_fd);
			printf("recv failed\n");
			abort();
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
		fds_t *cli_fd = (fds_t*)malloc(sizeof(fds_t));
		cli_fd->peer_fd = accept(fd, (struct sockaddr*)&remote, &len);
		char buffer[3] = {0};
		int bytes_received = 0;
		int total = 0;
		while (true) {
			printf("等待接收\n");
			bytes_received = nty_recv(cli_fd->peer_fd, buffer + total, sizeof(buffer) - total, 0);
			total += bytes_received;
			// 作协议解析
			// 如果不以\r\n结尾, 那么继续循环读，直到判断以\r\n结尾，那么接收了完整的数据，然后退出循环
			if (buffer[total - 2] == '\r' 
			&& buffer[total - 1] == '\n') {
				break;
			} else {
				printf("当前字符%c%c\n", buffer[total - 2], buffer[total - 1]);
			}
		}
		int choice = atoi(buffer);
		// 主从:0/r/n 
		// 客户端：1/r/n
		if (choice == 0) {
			// 主机同步连接，不需要回发
			// 设置成-10发送时判断是否大于0
			cli_fd->c_fd = -10;
		} else if (choice != 1) {
			printf("发送的模式错误\n");
			abort();
		} else {
			cli_fd->c_fd = cli_fd->peer_fd;
		}
		
		nty_coroutine *read_co;
		nty_coroutine_create(&read_co, server_reader, cli_fd);
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

extern volatile char* m_ip;

void replicate_slave(void *arg) {

	ports_t *ports = (ports_t*)arg;
	in_port_t p_task = ports->task_port;
	in_port_t p_ms = ports->m_port;
	// 连接主机发送当前任务的端口号
	int sock_fd = socket(AF_INET, SOCK_STREAM, 0);
	if (sock_fd < 0) {
		perror("socket creation failed");
		abort();
		exit(-13);
	}
	struct sockaddr_in serv_addr = {0};
	serv_addr.sin_family = AF_INET;
	serv_addr.sin_port = htons(p_ms);
	if (inet_pton(AF_INET, (const char*)m_ip, & serv_addr.sin_addr) <= 0) {
		perror("invalid address");
		close(sock_fd);
		abort();
		exit(-13);
	}
	printf("连接主机\n");
	int ret = connect(sock_fd, (struct sockaddr*)&serv_addr, sizeof(serv_addr));
	printf("连接成功\n");
	if (ret < 0) {
		perror("connect failed");
		// 详细错误处理
	  switch(errno) {
		  case ECONNREFUSED:
			  printf("No service listening on port %u\n", p_ms);
			  break;
		  case ETIMEDOUT:
			  printf("Connection timeout\n");
			  break;
		  case ENETUNREACH:
			  printf("Network unreachable\n");
			  break;
		  default:
			  printf("Error code: %d\n", errno);
	  }
	  close(sock_fd);
	  abort();
	  exit(-15);
	}
	char temp_buf[16] = {0};
	int actual_size = snprintf(temp_buf, sizeof(temp_buf), "%u\r\n", p_task);
	if (actual_size >= sizeof(temp_buf)) {
		perror("端口号过长");
		abort();
		exit(-14);
	}
	printf("开始发送\n");
	ssize_t r_sd = nty_send(sock_fd, temp_buf, strlen(temp_buf), 0);
	if (r_sd != strlen(temp_buf)) {
		perror("send发送失败, exit");
		abort();
		exit(-14);
	}
	printf("send port <->: %s\n", temp_buf);
	printf("端口发送成功\n");
	ssize_t r_rv = nty_recv(sock_fd, temp_buf, 1, 0);
	if (r_rv == 0) {
		close(sock_fd);
		return;
	} else {
		perror("recv接收错误");
		abort();
		exit(-11);
	}
	printf("replicate_slave 建立连接成功\n");
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
		printf("等待接收\n");
		bytes_received = nty_recv(pd->current_fd, buffer + total, sizeof(buffer) - 1 - total, 0);
		total += bytes_received;
		// 作协议解析
		// 如果不以\r\n结尾, 那么继续循环读，直到判断以\r\n结尾，那么接收了完整的数据，然后退出循环
		if (buffer[total - 2] == '\r' 
		&& buffer[total - 1] == '\n') 
		{
			break;
		} else {
			printf("当前字符%c%c\n", buffer[total - 2], buffer[total - 1]);
		}
	}
	printf("成功接收到从机端口信息\n");

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
	free(pd);
	int slave_fd = socket(AF_INET, SOCK_STREAM, 0);
	if (slave_fd < 0) {
        perror("socket creation failed");
		printf("!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!\n");
		printf("!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!\n");
		printf("!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!\n");
        return ;
    }
	printf("连接从机的任务端口\n");
	int ret = connect(slave_fd, (struct sockaddr*)&slave_addr, sizeof(slave_addr));
	if (ret < 0) {
		perror("connection failed");
		printf("!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!\n");
		printf("!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!\n");
		printf("!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!\n");
		printf("!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!\n");
        close(slave_fd);
        return ;
	}
	printf("连接从机的任务端口成功\n");
	// 连接成功，那么遍历内存中的数据
	// 组织成正确的协议格式发送给对端
	// 这里的内存数据结构是b树，那么如何遍历呢，还是需要一个类似的迭代器
	// 去指向每一个数据
//! 这里的迭代器遍历不是安全的，这里先只连接一个，如果是多个的话，应该用锁保护
//! 并且这里遍历的时候相当于是读操作了对于
	printf("开始发送模式\n");
    char temp_buf[16] = "0\r\n";
	ssize_t r_sd = nty_send(slave_fd, temp_buf, strlen(temp_buf), 0);
	if (r_sd != strlen(temp_buf)) {
		perror("send发送失败, exit");
		abort();
		exit(-14);
	}
	pthread_rwlock_wrlock(&global_m_btree->rwlock);
	printf("锁定内存的b树成功，用迭代器遍历构造请求包发送给从机\n");
	btree_iterator_t *iterator = create_iterator(global_m_btree);
	char buf[64] = {0};
	int used = 0;
	// 按照协议格式构造请求send给slave
	// 这里发送set请求
	// [lenth]set key val[lenth]set key val
	while (iterator->current->state != ITER_STATE_END) {
		bkey_t key_val = iterator_get(iterator);
		printf("获取内存的b树的键值,key:%s,val:%d\n", key_val.key, *(int*)key_val.data_ptrs);
		
		used = snprintf(buf + sizeof(int), sizeof(buf) - sizeof(int), "set %s %d", key_val.key, *(int*)key_val.data_ptrs);
		if (used >= sizeof(buf) - sizeof(int)) {
			perror("发送的消息过长,exit退出\n");
			printf("!!!!!!!!!!!!!!!!!!!!!\n");
			printf("!!!!!!!!!!!!!!!!!!!!!\n");
			printf("!!!!!!!!!!!!!!!!!!!!!\n");
			abort();
			exit(-13);
		}
		uint32_t temp_length = htonl(strlen(buf + 4));
		memcpy(buf, &temp_length, sizeof(uint32_t));
		
		int send_byte = nty_send(slave_fd, buf, strlen(buf + 4) + 4, 0);
		if (send_byte != strlen(buf + 4) + 4) {
			printf("nty_send 未发送完\n");
			abort();
		}
		
        iterator_find_next(iterator);
    }
	printf("同步完成\n");
	// 记录和保存当前fd，用于后续的同步操作,这里先用最简单的数组保存,后续可以用哈希存储
	int idx = atomic_load_explicit(&slave_cnt, memory_order_relaxed);
	if (idx > 9) {
		perror("slave 数量过多, exit退出\n");
		exit(-14);
	}

	slave_array[slave_cnt] = slave_fd;
	atomic_store_explicit(&slave_cnt, idx + 1, memory_order_release);
	printf("添加从机fd到数组中, 便于转发请求\n");
	pthread_rwlock_unlock(&global_m_btree->rwlock);
	printf("同步完成\n");
}
// 
// 主机运行该函威
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
		printf("!接收到从机连接!\n");
		nty_coroutine *pro_mst;
		process_data_t *pd = (process_data_t*)malloc(sizeof(process_data_t));
		pd->current_fd = cli_fd;
		pd->s_addr = remote.sin_addr.s_addr;
		nty_coroutine_create(&pro_mst, process_master, pd);
	}
}

//true master,false slave
extern volatile bool stage;
int NtyCo_Entry(unsigned short p, unsigned short p2) {
	size_t size = sizeof(kv_type_t);

	// small_kv_pool = fixed_pool_create(SMALL_SIZE, 3000000);
	// small_value_pool = fixed_pool_create(MEDIUM_SIZE, 3000000);
	// 当存储值时，才去alloc分配块，并且alloc时把

	// g_kv_array = (kv_type_t*)malloc(sizeof(kv_type_t) * KV_ARRAY_SIZE);
	printf("8\n");
	unsigned short *port = (unsigned short*)malloc(sizeof(unsigned short));
	*port = p;
	unsigned short *m_port = (unsigned short*)malloc(sizeof(unsigned short));
	*m_port = p2;

	nty_coroutine *co = NULL;
	nty_coroutine_create(&co, server, port);
	nty_coroutine *ms_co = NULL;
	if (stage == true) {
		nty_coroutine_create(&ms_co, replicate, m_port);
	} else {
		ports_t *ports = (ports_t*)malloc(sizeof(ports_t));
		ports->m_port = *m_port;
		ports->task_port = *port;
		nty_coroutine_create(&ms_co, replicate_slave, ports);
	}
	printf("9\n");
	nty_schedule_run();
	printf("10\n");
	return 0;
}




