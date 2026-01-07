#include "m_btree.h"
#include <stdio.h>
#include <stdlib.h>
btree_t *g_tree = NULL;
#define t 64


// insert test
// void test1() {
//     printf("------------ 1 ------------\n");
//     g_tree = btree_create(t);
//     for (int i = 0; i < 10; i++) {
//         btree_insert(g_tree, i);
//     }
//     printf("------------ 1end ------------\n");

//     printf("------------ 2 -------------\n");
//     for (int i = 0; i < 10; i++) {

//         search_result_t ret = btree_search(g_tree, i);
//         if (ret.found == true) {
//             printf("val:%d, found:%d, index:%d\n", i, ret.found, ret.index);
//         } else {
//             printf("val is not exist\n");
//         }
//     }
//     printf("------------ 2end -------------\n");
    
//     printf("------------- inorder ------------------\n");
//     btree_inorder_traversal(g_tree);
//     printf("\n"); 
// }

// // remove test
// void test2() {
//     g_tree = btree_create(t);
//     for (int i = 0; i < 100; i++) {
//         btree_insert(g_tree, i);
//     }
//     printf("    ------ inorder --------\n");
//     btree_inorder_traversal(g_tree);
//     printf("\n    ---- inorder end ----\n");
//     printf("---------------- remove ------------------\n");
//     for (int i = 0; i < 5; i += 2) {
//         // 删除偶数
//         search_result_t ret = btree_search(g_tree, i);
//         printf("val:%d, found:%d\n", ret.node->keys[ret.index], ret.found);
//         btree_remove(g_tree, i);
//         ret = btree_search(g_tree, i);
//         if (ret.found) {
//             printf("删除%d ->", i);
//             printf("val:%d, found:%d\n", ret.node->keys[ret.index], ret.found);
//         } else {
//             printf("未找到key:%d\n", i);
//         }
//     }
//     printf("------------   inorder ------------\n");
//     btree_inorder_traversal(g_tree);
//     printf("\n------------ inorder end------------\n");
//     for (int i = 0; i < 5; i += 2) {
//         btree_insert(g_tree, i);
//     }
//     printf("------------   inorder ------------\n");
//     btree_inorder_traversal(g_tree);
//     printf("\n------------ inorder end------------\n");
// }

// void test3() {
//     g_tree = btree_create(t);
//     btree_insert(g_tree, 1);
//     printf("------------   inorder ------------\n");
//     btree_inorder_traversal(g_tree);
//     printf("\n------------ inorder end------------\n");
//     btree_remove(g_tree, 1);
//     printf("------------   after remove ------------\n");
//     btree_inorder_traversal(g_tree);
//     printf("=================================================\n");

//     btree_insert(g_tree, 1);
//     printf("----after insert -----------\n");
//     btree_inorder_traversal(g_tree);
//     printf("\n------------ -----------\n");
// }

// int* test_array = NULL;
// int test_index = 0;
// void left_move(int *array, int num, int end) {

//     for (int i = num; i <= end; i++) {
//         array[i - num] = array[i];
//     }
//     test_index -= num;
// }
// // 大量数据测试
// void test4() {
//     #define data_cnt 10000
//     test_index = data_cnt - 1;
//     int line = 0;
//     test_array = (int*)malloc(sizeof(int) * data_cnt);
//     g_tree = btree_create(t);
//     for (int i = 0; i < data_cnt; i++) {
//         btree_insert(g_tree, i);
//         test_array[i] = i;
//     }
//     #define remove_cnt 3000
//     for (int i = 0; i < remove_cnt; i++) {
//         btree_remove(g_tree, test_array[i]);
//     }
//     left_move(test_array, remove_cnt, test_index);
    
//     bool sign = true;
//     for(int i = 0; i < test_index + 1; i++) {
//         if(btree_contains(g_tree, test_array[i])) {
//             printf("val:%d exist <-> ", test_array[i]);
//             line++;
//             if (line == 10) {
//                 line %= 10;
//                 printf("\n");
//             }
//         } else {
//             printf("%d is not exist\n", test_array[i]);
//             sign = false;
//             break;
//         }
//     }
//     printf("\n");
//     if (sign == false) {
//         printf("\n!!数据有错误!!\n");
//     } else {
//         printf("\n数据全部核对正确\n");
//     }
//     line = 0;
//     for (int i = 0; i < remove_cnt; i++) {
//         if (!btree_contains(g_tree, i)) {
//             printf("不存在val:%d <-> ", i);
//             line++;
//             if (line % 10 == 0) {
//                 printf("\n");
//             }
//         } else {
//             printf("\n!!数据有错误!!\n");
//         }
//     }
//     printf("\n-----------------------\n");
//     printf("\n-----------------------\n");
//     printf("\n-----------------------\n");
//     printf("\n-----------------------\n");
//     printf("\n-----------------------\n");
//     // 测试结构是否正确，用中序遍历
//     // btree_inorder_traversal_test_array(g_tree, test_array);
//     printf("\n");

//     for (int i = 5000; i <= 7999; i++) {
//         btree_remove(g_tree, i);
//     }
//     btree_inorder_traversal(g_tree);
//     printf("\n-----------------------\n");
//     printf("\n-----------------------\n");
//     printf("\n-----------------------\n");
//     printf("\n-----------------------\n");
//     printf("\n-----------------------\n");
// }

// #include "../memory_pool/memory_pool.h"
// #include <stdint.h>
// uint32_t num = 0;

// void  test5() {
//     int_global_fixed_pool = fixed_pool_create(4, 1000111);
//     bkey_t temp = {0};
//     g_tree = btree_create(t);
//     for (int i = 1; i <= 1000000; i++) {
//         temp.data_ptrs = fixed_pool_alloc(int_global_fixed_pool);
//         *((int*)temp.data_ptrs) = i;
//         sprintf(temp.key, "key%d", i);
//         btree_insert(g_tree, temp, int_global_fixed_pool);
//     }
//     search_result_t result = {0};
//     for (int i = 1; i <= 1000000; i++) {
//         sprintf(temp.key, "key%d", i);
//         result = btree_search(g_tree, temp);
//         if (result.found == true) {
//             printf("%s : %d || ", result.node->keys[result.index].key,
//             *((int*)result.node->keys[result.index].data_ptrs));
//             num++;
//             if (num % 10 == 0) {
//                 printf("\n");
//             }
//         }
//     }

//     for (int i = 1234; i <= 1000000; i++) {
//         sprintf(temp.key, "key%d", i);
//         btree_remove(g_tree, temp, int_global_fixed_pool);
//     }

//     for (int i = 1; i <= 1000000; i++) {
//         sprintf(temp.key, "key%d", i);
//         result = btree_search(g_tree, temp);
//         if (result.found == true) {
//             printf("%s : %d || ", result.node->keys[result.index].key,
//             *((int*)result.node->keys[result.index].data_ptrs));
//             num++;
//             if (num % 10 == 0) {
//                 printf("\n");
//             }
//         } else {
//             // printf("%s not exist", temp.key)
//         }
//     }
    
// }


// int main() {
//     //  test1();
// //    test2();
//     // test3();
//     // test4();
//     test5();
// }



#include "../memory_pool/memory_pool.h"

#include "m_btree.h"
int array[10000];
int cnt = 0;
btree_t* global_btree;
int main() {
    int_global_fixed_pool = fixed_pool_create(4, 1000111);
    global_btree = btree_create(512);
    char *prefix = "key";
    for (int i = 0; i < 1000; i++) {
        bkey_t temp_key = {0};
        char temp_buf[16] = {0};
        sprintf(temp_buf + 3, "%d", i);
        memcpy(temp_buf, prefix, 3);
        temp_key.data_ptrs = fixed_pool_alloc(int_global_fixed_pool);
        *((int*)temp_key.data_ptrs) = i;
        temp_key.length = strlen(temp_buf + 3) + 3;
        memcpy(temp_key.key, temp_buf, temp_key.length);
        btree_insert(global_btree, &temp_key, int_global_fixed_pool);
    }
    //使用迭代器打印0-999的数据
    btree_iterator_t* ite = create_iterator(global_btree);
    for (int i = 0; i < 10000; i++) {
        array[i] = -1;
    }
    // 把值存放到数组中
    int cnt_n = 0;
    while(ite->current->state != ITER_STATE_END) {
        bkey_t temp_key = iterator_get(ite);
        array[cnt++] = *(int*)temp_key.data_ptrs;
        printf("插入值%d", *(int*)temp_key.data_ptrs);
        cnt_n++;
        if (cnt_n % 15 == 0) {
            printf("\n");
        }
        iterator_find_next(ite);
    }


    printf("=========================================");
    printf("=========================================");
    printf("=========================================");
    printf("=========================================");
    printf("=========================================");
    printf("=========================================");
    printf("=========================================");
    // 每次都遍历去查找值看是否有该值
    if (cnt != 1000) {
        abort();
    }
    for (int i = 0; i < cnt; i++) {
        int sign = 0;
        for (int j = 0; j < cnt; j++) {
            if (array[j] == i) {
                sign = 1;
                break;
            }
        }
        if (sign != 1) {
            printf("当前值没找到%d", i);
            abort();
        } else {
            printf("找到值%d", i);
        }
        // if (array[i] != i) {
        //     printf("数据错误, 当前i:%d,值%d\n", i, array[i]);
        //     abort();
        // }
    }
    printf("数据校验完成,cnt:%d\n", cnt);
    return 0;
}
















