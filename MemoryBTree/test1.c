#include "m_btree.h"
#include <stdio.h>
#include <stdlib.h>
btree_t *g_tree = NULL;
#define t 2


// insert test
void test1() {
    printf("------------ 1 ------------\n");
    g_tree = btree_create(t);
    for (int i = 0; i < 10; i++) {
        btree_insert(g_tree, i);
    }
    printf("------------ 1end ------------\n");

    printf("------------ 2 -------------\n");
    for (int i = 0; i < 10; i++) {

        search_result_t ret = btree_search(g_tree, i);
        if (ret.found == true) {
            printf("val:%d, found:%d, index:%d\n", i, ret.found, ret.index);
        } else {
            printf("val is not exist\n");
        }
    }
    printf("------------ 2end -------------\n");
    
    printf("------------- inorder ------------------\n");
    btree_inorder_traversal(g_tree);
    printf("\n"); 
}

// remove test
void test2() {
    g_tree = btree_create(t);
    for (int i = 0; i < 100; i++) {
        btree_insert(g_tree, i);
    }
    printf("    ------ inorder --------\n");
    btree_inorder_traversal(g_tree);
    printf("\n    ---- inorder end ----\n");
    printf("---------------- remove ------------------\n");
    for (int i = 0; i < 5; i += 2) {
        // 删除偶数
        search_result_t ret = btree_search(g_tree, i);
        printf("val:%d, found:%d\n", ret.node->keys[ret.index], ret.found);
        btree_remove(g_tree, i);
        ret = btree_search(g_tree, i);
        if (ret.found) {
            printf("删除%d ->", i);
            printf("val:%d, found:%d\n", ret.node->keys[ret.index], ret.found);
        } else {
            printf("未找到key:%d\n", i);
        }
    }
    printf("------------   inorder ------------\n");
    btree_inorder_traversal(g_tree);
    printf("\n------------ inorder end------------\n");
    for (int i = 0; i < 5; i += 2) {
        btree_insert(g_tree, i);
    }
    printf("------------   inorder ------------\n");
    btree_inorder_traversal(g_tree);
    printf("\n------------ inorder end------------\n");
}

void test3() {
    g_tree = btree_create(t);
    btree_insert(g_tree, 1);
    printf("------------   inorder ------------\n");
    btree_inorder_traversal(g_tree);
    printf("\n------------ inorder end------------\n");
    btree_remove(g_tree, 1);
    printf("------------   after remove ------------\n");
    btree_inorder_traversal(g_tree);
    printf("=================================================\n");

    btree_insert(g_tree, 1);
    printf("----after insert -----------\n");
    btree_inorder_traversal(g_tree);
    printf("\n------------ -----------\n");
}

int* test_array = NULL;
int test_index = 0;
void left_move(int *array, int num, int end) {

    for (int i = num; i <= end; i++) {
        array[i - num] = array[i];
    }
    test_index -= num;
}
// 大量数据测试
void test4() {
    #define data_cnt 10000
    test_index = data_cnt - 1;
    int line = 0;
    test_array = (int*)malloc(sizeof(int) * data_cnt);
    g_tree = btree_create(t);
    for (int i = 0; i < data_cnt; i++) {
        btree_insert(g_tree, i);
        test_array[i] = i;
    }
    #define remove_cnt 3000
    for (int i = 0; i < remove_cnt; i++) {
        btree_remove(g_tree, test_array[i]);
    }
    left_move(test_array, remove_cnt, test_index);
    
    bool sign = true;
    for(int i = 0; i < test_index + 1; i++) {
        if(btree_contains(g_tree, test_array[i])) {
            printf("val:%d exist <-> ", test_array[i]);
            line++;
            if (line == 10) {
                line %= 10;
                printf("\n");
            }
        } else {
            printf("%d is not exist\n", test_array[i]);
            sign = false;
            break;
        }
    }
    printf("\n");
    if (sign == false) {
        printf("\n!!数据有错误!!\n");
    } else {
        printf("\n数据全部核对正确\n");
    }
    line = 0;
    for (int i = 0; i < remove_cnt; i++) {
        if (!btree_contains(g_tree, i)) {
            printf("不存在val:%d <-> ", i);
            line++;
            if (line % 10 == 0) {
                printf("\n");
            }
        } else {
            printf("\n!!数据有错误!!\n");
        }
    }
    printf("\n-----------------------\n");
    // btree_inorder_traversal(g_tree);
    printf("\n");
}

int main() {
    //  test1();
//    test2();
    // test3();
    test4();
}