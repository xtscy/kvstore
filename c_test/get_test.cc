#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <iostream>
#include <cstring>
#include <thread>
#include <unistd.h>
#include <set>
#include <string>

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
            printf("connect shutdown by peer\n");
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

    // 发送get请求
    
    if (argc != 5) {
        std::cout << "./get_test ip port begin_num1 end_num2" << std::endl;
        return 0;
    }

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
    std::cout << "发送模式成功" << std::endl;
    int begin_num1 = atoi(argv[3]);
    int end_num2 = atoi(argv[4]);

    char send_buf[64] = {0};
    // send get 请求
    // 单次的send然后recv判断值是否正确
    uint32_t len = 0;
    int cnt = end_num2 - begin_num1 + 1;
    int ccnt = 0;

    std::set<int> s;
    std::cout << "开始发送get" << std::endl;
    for (int i = begin_num1; i <= end_num2; i++) {
        int rsp = snprintf(send_buf + 4, sizeof(send_buf) - 4, "get tkey%d", i);
        // std::cout << "send-> " << buf + 4 << std::endl;
        // printf("rsp:%d\n", rsp);
        if (rsp >= sizeof(send_buf) - 4) {
            throw std::runtime_error("int rsp = snprintf(buf + 4, sizeof(buf) - 4, \"set tkey%d %d\", i, i);");
        }

        // buf[rsp + 4] = '\0';
        len = htonl(strlen(send_buf + 4));
        memcpy(send_buf, &len, sizeof(uint32_t));
        ssend(nfd, send_buf, strlen(send_buf + 4) + 4, 0);
        // std::cout << "当前get发送成功" << std::endl;
        char buf[64] = {0};
        int cur_end = 0;

        //                                                                                                  //

        while (true) {
            // std::cout << "while" << std::endl;
            // 这里可能接收不完整所以while循环,当判断到有\r\n时说明接收了一个完整请求
            // 然后放到set集合中,那么这里其实还有可能接收到的并不是一个完整的请求
            // *这里从cur_end位置进行一个读取覆盖\0
            int recv_len = recv(nfd, buf + cur_end, sizeof(buf) - 1 - cur_end, MSG_DONTWAIT);
            if (recv_len == -1) {
                // 这里是没有数据,那么这里的退出条件不再读的条件是读完了
                // 用ccnt来判断,用cur_end来判断,cur_end当前有数据,那么继续走下面的
                // 因为有可能也有两个包,如果cur_end当前没数据,同时ccnt不满足条a件那么继续循环直接continue
                // 如果当前没数据,并且ccnt已经满足条件了,说明当前数据接收完成直接退出当前的循环
                // 这里总结一下就是,ccnt不满足那么继续循环,走下面还是直接continue由cur_end决定
                // 如果ccnt已经满足那么直接跳出循环
                if (ccnt == cnt) break;
                else if (cur_end == 0) continue;
                recv_len = 0;
            }
            // 接收到数据,或有剩余的数据
            if (recv_len + cur_end > sizeof(buf) - 1) {
                abort();
            }
            buf[recv_len + cur_end] = '\0';
            //* pos指向当前/r,cur_end指向当前的
            //* 这里接收的包的数据可能上一个包就有,所以这里不能用recv_len而是当前的总的一个长度
            //* 这里, 总长度应该是,上一个包覆盖前面数据后的结束标志 + 当前的接收长度,上一个包的结束标志位置就是cur_end
            //* cur_end指向\0, cur_end同时是前面的元素个数,recv_len为接收到的字节个数
            int pos = 0;
            while (pos < cur_end + recv_len - 1) {
                // 没有拿到完整的一个请求这里
                if (buf[pos] == '\r' && buf[pos + 1] == '\n') {
                    buf[pos] = '\0';
                    ccnt++;
                    break;//跳出时pos指向\r位置
                }
                pos++;
            }
            // 这里有可能上面没有接收到但是有数据,但是这个数据不完整,所以遍历到cur_end + recv_len
            // 那么对于这种情况也是直接continue,但是如果该大小已经和数组大小 - 1相同说明当前数组放不下同时
            // 也没有\r\n那么说明接收的字符过长,那么这里直接abort
            if (pos + 1 == sizeof(buf) - 1) {
                abort();
            }
            // 如果pos没有到达最后,那么判断是否到达了cur_end+recv_len
            // 如果到达了说明当前数据没有\r\n结尾,不是完整继续读,如果读到一个完整的那么pos必然指向\r的位置不等于cur_end + recv_len
            // 而是等于cur_end + recv_len
            if (pos + 1 == cur_end + recv_len) {
                std::cout << "pos + 1 == cur_end + recv_len" << std::endl;
                cur_end = cur_end + recv_len;
                continue;
            }
            // 当前读取到了一个完整的包,并且当前的pos指向\r
            buf[pos] = '\0';
            //* 把当前值转换成int放入set中,或者直接放入一个std::string
            int val = ::atoi(buf);
            // set
            auto [iter1, success1] = s.insert(val);  // C++17结构化绑定
            if (success1) {
                // * 这里上面只发送了一个请求说明只会插入成功1个数据
                // * 这里没有单独开线程所以直接break跳出当前的recv的循环
                // * 这里也可以两个都是不同的线程
                //* 这里不会接收到下一个包的消息，在下一个包发送之前
                // * 所以可以直接break然后重新读取
                std::cout << "插入成功: " << *iter1 << std::endl;
                break;
            } else {
                std::cout << "插入失败，值已存在" << *iter1 << std::endl;
                abort();
            }
            // 如果在当前包的后面存在数据那么copy到前面
            // pos指向/r,如果有数据那么应该是pos + 1 + 1 = pos + 2指向下一个请求的头
            // 结束位置为,当前总字符串的结束位置,即cur_end + recv_len ,即当前末尾 + 当前接收字符,cur_end + recv_len
            // 拷贝完后,又计算新的末尾
            std::copy(buf + pos + 2, buf + cur_end + recv_len, buf);
            cur_end = cur_end + recv_len - pos + 2;
        }
    }

    int sign = 0;
    
    
    for (int i = begin_num1; i <= end_num2; i++) {

        if (s.count(i)) {
            // std::cout << "存在值:" << i << std::endl;
        } else {
            std::cout << "不存在值:" << i << std::endl;
            sign = 1;
            abort();
        }
    }

    if (sign) {
        std::cout << "-------------------------" << std::endl;
        std::cout << "校验失败" << std::endl;
        std::cout << "-------------------------" << std::endl;
    } else {
        std::cout << "-------------------------" << std::endl;
        std::cout << "校验成功,通过测试" << std::endl;
        std::cout << "-------------------------" << std::endl;
    }
    
    
    cmd_thread.join();
    return 0;    
    
    // std::thread recv_thread([&nfd] {
    //     char recv_buf[64] = {0};
    //     int recv_len = 0;
    //     int cnt = 0;
    //     while (true) {
    //         cnt++;

    //         std::cout << "recv cnt:" << cnt << std::endl;
    //         recv_len = recv(nfd, recv_buf, sizeof(recv_buf) - 1, 0);
    //         // std::cout << "recv_len: " << recv_len << std::endl;
    //         if (recv_len > 0) {
    //             recv_buf[recv_len] = '\0';
    //             std::cout << "recv->" << recv_buf << " " << std::endl; 
    //         } else {
    //             std::cout << "recv 0 or <0" << std::endl;
    //             abort();
    //             // throw std::runtime_error("recv 0 or <0");
    //             return ;
    //         }
    //     }
    // });
        // char buf[64] = {0};
        // // send get 请求
        // // 单次的send然后recv判断值是否正确
        // uint32_t len = 0;
        // for (int i = begin_num1; i <= end_num2; i++) {
        //     int rsp = snprintf(buf + 4, sizeof(buf) - 4, "get tkey%d", i);
        //     // std::cout << "send-> " << buf + 4 << std::endl;
        //     // printf("rsp:%d\n", rsp);
        //     if (rsp >= sizeof(buf) - 4) {
        //         throw std::runtime_error("int rsp = snprintf(buf + 4, sizeof(buf) - 4, \"set tkey%d %d\", i, i);");
        //     }

        //     // buf[rsp + 4] = '\0';
        //     len = htonl(strlen(buf + 4));
        //     memcpy(buf, &len, sizeof(uint32_t));
        //     ssend(nfd, buf, strlen(buf + 4) + 4, 0);
        // }
        // std::cout << "get_send_thread run end" << std::endl;
        // recv_thread.join();
}
    

