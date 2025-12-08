#include <stdio.h>
#include <string.h>

int main() {
    const char* message = "get key2 get key1 get key2";
    unsigned int length = strlen(message);
    
    // 打开文件进行二进制写入
    FILE* file = fopen("data.bin", "wb");
    if (!file) {
        perror("文件打开失败");
        return 1;
    }
    
    // 直接写入4字节长度（使用系统原生字节序）
    size_t written = fwrite(&length, sizeof(unsigned int), 1, file);
    if (written != 1) {
        perror("写入长度失败");
        fclose(file);
        return 1;
    }
    
    // 写入字符串数据（不包括结束符）
    written = fwrite(message, sizeof(char), length, file);
    if (written != length) {
        perror("写入数据失败");
        fclose(file);
        return 1;
    }
    
    fclose(file);
    
    printf("数据已成功写入 data.bin\n");
    printf("长度: %u bytes\n", length);
    printf("内容: %s\n", message);
    
    // 显示文件内容（十六进制格式）
    printf("\n文件内容（十六进制）:\n");
    file = fopen("data.bin", "rb");
    if (file) {
        int byte;
        while ((byte = fgetc(file)) != EOF) {
            printf("%02X ", byte);
        }
        fclose(file);
        printf("\n");
    }
    
    return 0;
}