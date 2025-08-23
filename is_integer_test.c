#include <errno.h>
#include <stddef.h>
#include <stdio.h>
#include <stdlib.h>

int is_integer(const char *str) {
    printf("str:%s\n", str);
    if (str == NULL || *str == '\0') {
        printf("不是integer\n");
        return 0;
    }
    
    char *endptr = NULL;
    errno = 0; // 必须重置errno
    long value = strtol(str, &endptr, 10); // 10表示十进制
    printf("errno:%d\n", errno);
    // 三个条件必须同时满足：
    // 1. 整个字符串都被成功转换 (endptr指向字符串结束符)
    // 2. 没有发生溢出错误 (errno保持为0)
    // 3. 至少转换了一个字符 (避免空字符串或纯符号)
    printf("*endptr:%c, errno:%d,endptr:%p\n", *(endptr), errno, endptr);
    return (*endptr == '\0' && errno == 0 && endptr != str);
}

int main() {
        char *str = "SET KET VALUE";
    is_integer(str);
    return 0;
}