#include <stdio.h>
#include "bpt_c.h"
int main() {
    btree_handle btree = btree_create_c();
    // bpt::bplus_tree *bpt = new bpt::bplus_tree("./test1.db", false);
    // std::string temp = "KEY";
    char buf[30] = {0};
    for (int i = 0; i < 1000000; i++) {
        sprintf(buf, "KEY%d", i);
        btree_insert_c(btree, buf, i);
        if (i % 1000 == 0) {
            printf("data:%d\n", i);
        }
    }
   

    int anser = 0;
    // std::string str = "KEY";
    for (int i = 0; i < 1000000; i++) {

        sprintf(buf, "KEY%d", i);
        anser = btree_search_c(btree, buf);

        // int ret = bpt->search(bpt::key_t((str + std::to_string(i)).c_str()), &anser);
        // if (ret == 0) {
        printf("%s: %d", buf, anser);
    }
    return 0;
}
// int main1() {
//     btree_handle btree = btree_create_c();
//     printf("create success\n");
//     int ret = btree_insert_c(btree, "KEY1", 1);
//     if (ret == 0) {
//         printf("insert success: KEY1, 1\n");
//     }

//     int val = btree_search_c(btree, "KEY1");

//     printf("KEY1:%d\n", val);    
//     ret = btree_insert_c(btree, "KEY2", 200);
//     if (ret == 0) {
//         printf("insert success: KEY1, 1\n");
//     }

//      val = btree_search_c(btree, "KEY2");

//     printf("KEY2:%d\n", val);    
    
//     return 0;
    
// }