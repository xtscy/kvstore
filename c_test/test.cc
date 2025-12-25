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

    // 发送set指令
    // 这里先构建请求
    // 客户端连接服务端是直接初始化网络信息然后bind网络信息最后连接
    //2. init addr_in 3. bind 4. connect 5.send

    if (argc != 5) {
        std::cout << "./test ip port begin_num1 end_num2" << std::endl;
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
    
    int begin = atoi(argv[3]);
    int end = atoi(argv[4]);
    
    std::thread send_thread([&nfd, &begin, &end] {
        char buf[64] = {0};
        uint32_t len = 0;
        int i = 0;
        for (i = begin; i <= end; i++) {
            memset(buf, 0, sizeof(buf));
            int rsp = snprintf(buf + 4, sizeof(buf) - 4, "set tkey%d %d", i, i);
            // printf("rsp:%d\n", rsp);
            if (rsp >= sizeof(buf) - 4) {
                throw std::runtime_error("int rsp = snprintf(buf + 4, sizeof(buf) - 4, \"set tkey%d %d\", i, i);");
            }
            // buf[4 + rsp] = '\0';
            len = htonl(strlen(buf + 4));
            memcpy(buf, &len, 4);
            if (std::string(buf + 4) == "set tkey") {
                // std::cout << "std::string(buf + 4) == \"set tkey\"" << std::endl;
                throw std::runtime_error("send set tkey");
            }
            // std::cout << "strlen(buf) " << "strlen(buf) " <<strlen(buf) << std::endl;
            // std::cout << "ssend behing" << std::endl;
            ssend(nfd, buf, strlen(buf + 4) + 4, 0);
            // std::cout << "buf:" << buf + 4 << std::endl;
            // usleep(100000);
        }
        std::cout << "send_thread run end" << std::endl;
    });

    std::thread recv_thread([&nfd] {
        char recv_buf[64] = {0};
        int recv_len = 0;
        int cnt = 0;
        while (true) {
            cnt++;
            std::cout << "recv cnt:" << cnt << std::endl;
            recv_len = recv(nfd, recv_buf, sizeof(recv_buf) - 1, 0);
            // std::cout << "recv_len: " << recv_len << std::endl;
            if (recv_len > 0) {
                recv_buf[recv_len] = '\0';
                std::cout << recv_buf << " " << std::endl;; 
            } else {
                std::cout << "recv 0 or <0" << std::endl;
                // throw std::runtime_error("recv 0 or <0");
                return ;
            }
        }
    });
        //? from_chars专用于高性能的字符串转整形
        //? auto [ptr, ec] = std::from_chars(input.data(), 
        //?                                 input.data() + input.size(), 
        //?                                 result);
        //? if (ec == std::errc()) {
        //?     // 转换成功
        //? }

    
    // ?  c++20
    // ? 使用 <=> 三路比较运算符
    // ? auto operator<=>(const MyClass& other) const = default;
    // ? 编译器自动生成：!=, <, <=, >, >=
    // 只需要写2个重载一个== 一个<=> 
    // ? auto result = str1 <=> str2;

    // ? starts_with和ends_with
    // ? if (str.starts_with("hello")) {
    // ?     std::cout << "以 hello 开头\n";
    // ? }
    // ? 
    // ? if (str.ends_with("world")) {
    // ?     std::cout << "以 world 结尾\n";
    // ? }
    
    // ? string_view 比较（C++17）
    

    
    std::thread cmd_thread([] {
        std::string input;
        std::string_view cmd("quit");
        std::getline(std::cin, input);

        auto result = input <=> cmd;
        if (result == std::strong_ordering::less) {

            std::cout << "cmd not exist\n请重新输入" << std::endl;
            
        } else if (result == std::strong_ordering::equal) {

            std::cout << "quit!" << std::endl;
            exit(0);
            
        } else if (result == std::strong_ordering::greater) {
            std::cout << "cmd not exist\n请重新输入" << std::endl;
        }
        
        
    });
    
    send_thread.join();
    recv_thread.join();
    cmd_thread.join();
    return 0;
}