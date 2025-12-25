#ifndef BTREE_C_H
#define BTREE_C_H

#ifdef __cplusplus
extern "C" {
#endif
    // 定义不透明的指针（C语言看不到具体实现）
typedef void* btree_handle;

// 创建和销毁
btree_handle btree_create_c(void);
// void btree_destroy(btree_handle tree);

// 基本操作
int btree_insert_c(btree_handle handle, const char* key, const int value);


int btree_search_c(btree_handle handle, const char* key, int* val);
// int btree_search(btree_handle tree, int key, char* buffer, int buffer_size);
// int btree_remove(btree_handle tree, int key);
// int btree_size(btree_handle tree);

#ifdef __cplusplus
}
#endif

#endif // BTREE_C_H
