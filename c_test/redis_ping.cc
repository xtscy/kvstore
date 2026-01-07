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




    // 发送set指令
    // 这里先构建请求
    // 客户端连接服务端是直接初始化网络信息然后bind网络信息最后连接
    //2. init addr_in 3. bind 4. connect 5.send

 #define PORT 5566

int main() {
    // 1. 创建 socket
    int server_fd = socket(AF_INET, SOCK_STREAM, 0);
    if (server_fd < 0) {
        perror("socket failed");
        exit(EXIT_FAILURE);
    }
    
    // 2. 设置 SO_REUSEADDR 选项（避免 Address already in use）
    int opt = 1;
    if (setsockopt(server_fd, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt)) < 0) {
        perror("setsockopt failed");
        close(server_fd);
        exit(EXIT_FAILURE);
    }
    
    // 3. bind - 绑定地址和端口
    struct sockaddr_in address;
    memset(&address, 0, sizeof(address));
    address.sin_family = AF_INET;
    address.sin_addr.s_addr = INADDR_ANY;  // 监听所有网卡
    address.sin_port = htons(PORT);        // 端口 5555
    
    if (bind(server_fd, (struct sockaddr*)&address, sizeof(address)) < 0) {
        perror("bind failed");
        close(server_fd);
        exit(EXIT_FAILURE);
    }
    
    printf("Socket bound to port %d\n", PORT);
    
    // 4. listen - 开始监听
    if (listen(server_fd, 5) < 0) {  // 5 是等待队列长度
        perror("listen failed");
        close(server_fd);
        exit(EXIT_FAILURE);
    }
    
    printf("Listening on port %d...\n", PORT);
    
    // 5. accept - 接受连接
    struct sockaddr_in client_addr;
    socklen_t client_len = sizeof(client_addr);
    
    int client_fd = accept(server_fd, (struct sockaddr*)&client_addr, &client_len);
    if (client_fd < 0) {
        perror("accept failed");
        close(server_fd);
        exit(EXIT_FAILURE);
    }
    int nfd = client_fd;
    std::thread recv_thread([&nfd] {
        char recv_buf[64] = {0};
        int recv_len = 0;
        int cnt = 0;
        while (true) {
            cnt++;
            // std::cout << "recv cnt:" << cnt << std::endl;
            recv_len = recv(nfd, recv_buf, sizeof(recv_buf) - 1, 0);
            // std::cout << "recv_len: " << recv_len << std::endl;
            if (recv_len > 0) {
                recv_buf[recv_len] = '\0';
                std::cout << recv_buf << " " << std::endl;
            } else {
                std::cout << "recv 0 or <0" << std::endl;
                // throw std::runtime_error("recv 0 or <0");
                return ;
            }
        }
    });
    recv_thread.join();
}