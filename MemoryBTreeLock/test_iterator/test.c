#include "../m_btree.h"
#include <stdlib.h>
#include <unistd.h>
btree_t *global_tree;
int cnt;
extern fixed_size_pool_t *int_global_fixed_pool;

int main() {
    global_tree = btree_create(512);
    int_global_fixed_pool = fixed_pool_create(sizeof(int), 1000111);
    // 添加数据
    for (int i = 1; i <= 500; i++) {
        void *dptr = fixed_pool_alloc(int_global_fixed_pool);
        *((int*)dptr) = i;
        bkey_t temp_key = {0};
        temp_key.data_ptrs = dptr;
        sprintf(temp_key.key, "key%d", i);
        btree_insert(global_tree, temp_key, int_global_fixed_pool);
    }

    // btree_inorder_traversal(global_tree);
    printf("---------------------------\n");
    printf("===========================\n");
    printf("===========================\n");
    printf("---------------------------\n");
    // sleep(5);
    btree_iterator_t *ite = create_iterator(global_tree);
    while (ite->current->state != ITER_STATE_END) {
        
        bkey_t key_val = iterator_get(ite);
        printf("key: %s,val: %d |||  ", key_val.key, *(int*)key_val.data_ptrs);
        if (++cnt % 5 == 0) {
            printf("\n");
        }
        iterator_find_next(ite);
    }
    printf("\n");
    return 0;
}