#include <stdio.h>
#include "../../memory_pool/memory_pool.h"





fixed_size_pool_t *g_mem_pool = NULL;

#define MEMORY_SIZE 50


int main() {
    //* 每个块大小为64
    g_mem_pool = fixed_pool_create(MEMORY_SIZE, 5);
    char *block_data = (char*)fixed_pool_alloc(g_mem_pool);
    fixed_pool_stats(g_mem_pool);
    char *data = (char*)fixed_pool_alloc(g_mem_pool);
    memset(data, 0, MEMORY_SIZE);
    char buf[64] = {0};
    snprintf(buf, sizeof(buf), "hello world\n");
    size_t copy_size = sizeof(buf) < MEMORY_SIZE ? sizeof(buf) : MEMORY_SIZE;
    memcpy(data, buf, copy_size);
    printf("store msg: %s\n", data);
    fixed_pool_stats(g_mem_pool);
    printf("sleep 2s\n");
    sleep(2);
    fixed_pool_destroy(g_mem_pool);
    printf("destroy mem_pool\n");
    return 0;
}