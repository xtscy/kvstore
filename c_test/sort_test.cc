#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <iostream>
#include <cstring>
#include <thread>
#include <unistd.h>
#include <compare> // <=>

ssize_t ssend(int fd, const void *buf, size_t len, int flags) {

	int sent = 0;
	while (sent < len) {
        // printf("sent:%d, len:%lu\n", sent, len);
        // std::cout << "send behind" << std::endl;
		int ret = send(fd, ((char*)buf)+sent, len-sent, flags);
        // std::cout << "send after" << std::endl;
		if (ret < 0) {			
            // printf("当前send_f出错,ret=%d,continue\n", ret);
            return -1;
		} else if (ret == 0) {
            // printf("connect shutdown by peer\n");
            return ret;
        } else {
            // printf("ret=%d\n", ret);
            // printf("send->:%s|len->:%lu", (char*)buf, len);
        }
		sent += ret;
	} 
    if (sent == len) return sent;
    else {
        printf("当前信息未发送完,eixt(-11)\n");
        exit(-11);
    }

}


int main(int argc, char* argv[]) {
   if (argc != 4) {
        std::cout << "./test ip port sort_num" << std::endl;
        return 0;
    }

    int nfd = socket(AF_INET, SOCK_STREAM, 0);
    sockaddr_in addr = {0};
    addr.sin_family = AF_INET;
    inet_pton(AF_INET, argv[1], &addr.sin_addr);
    in_port_t port = atoi(argv[2]);
    addr.sin_port = htons(port);// 转网络序
    if (connect(nfd, (struct sockaddr*)&addr, sizeof(addr)) < 0) {
        perror("连接失败");
        close(nfd);
        return -1;
    }

	printf("开始发送模式\n");
    char temp_buf[16] = "1\r\n";
	ssize_t r_sd = ssend(nfd, temp_buf, strlen(temp_buf), 0);
	if (r_sd != strlen(temp_buf)) {
		perror("send发送失败, exit");
		abort();
		exit(-14);
	}
    
    int num = atoi(argv[3]);

    char buf[64] = {0};
    uint32_t len = 0;
    int i = 0;
    memset(buf, 0, sizeof(buf));
    int rsp = snprintf(buf + 4, sizeof(buf) - 4, "sort %d", num);
    if (rsp >= sizeof(buf) - 4) {
        throw std::runtime_error("int rsp = snprintf(buf + 4, sizeof(buf) - 4, \"set tkey%d %d\", i, i);");
    }
    len = htonl(strlen(buf + 4));
    memcpy(buf, &len, 4);
    ssend(nfd, buf, strlen(buf + 4) + 4, 0);

    std::thread recv_thread([&nfd] {
        char recv_buf[64] = {0};
        int recv_len = 0;
        int cnt = 0;
        while (true) {
            cnt++;
            std::cout << "recv cnt:" << cnt << std::endl;
            recv_len = recv(nfd, recv_buf, sizeof(recv_buf) - 1, 0);
            if (recv_len > 0) {
                recv_buf[recv_len] = '\0';
                std::cout << recv_buf << " " << std::endl;; 
            } else {
                std::cout << "recv 0 or <0" << std::endl;
                return ;
            }
        }
    });
    recv_thread.join();
}