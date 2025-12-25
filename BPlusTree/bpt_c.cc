#include "bpt.hpp"
#include "bpt_c.h"




extern "C" {

struct btree_wrapper {
    bpt::bplus_tree* tree;
};

btree_handle btree_create_c(void) {
    // printf("btree_create_c 1\n");
    btree_wrapper* wrapper = new btree_wrapper();
    // printf("btree_create_c 2\n");
    wrapper->tree = new bpt::bplus_tree("kvtest.db", false);
    // printf("btree_create_c 3\n");

    return static_cast<btree_handle>(wrapper);
}
 
int btree_insert_c(btree_handle handle, const char* key, const int value) {
    if (handle == nullptr) return -1;
    
    btree_wrapper* wrapper = static_cast<btree_wrapper*>(handle);
    
    // try {
    //     bool success = wrapper->tree->insert(key, std::string(value));
    //     return success ? 0 : -1;
    // } catch (...) {
    //     return -1;
    // }

    int ret = wrapper->tree->insert(bpt::key_t(key), value);
    return ret;
    // 1为重复，0为成功insert, -1为参数错误
}

int btree_search_c(btree_handle handle, const char* key, int* val){
    if (handle == nullptr) return -1;
    
    btree_wrapper* wrapper = static_cast<btree_wrapper*>(handle);
    bpt::value_t r_val = 0;
    int ret = wrapper->tree->search(bpt::key_t(key), &r_val);
    
    if (ret != 0) {
        std::cout << "search failed, exit -1" << std::endl;
        return -1;
    }
    *val = r_val;
    return 0;
}



}