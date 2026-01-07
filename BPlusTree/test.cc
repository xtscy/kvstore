#include "bpt.hpp"
#include "predefined.hpp"
#include <iostream>
#include <string>
#include <set>
int main() {


    bpt::bplus_tree *bpt = new bpt::bplus_tree("./test1.db", false);
    std::string temp = "KEY";
    std::cout << "insert..." << std::endl;
    for (int i = 0; i < 10000; i++) {
        std::string temp_str (temp + std::to_string(i));
        std::cout << temp_str << "len:" << temp_str.size() << std::endl;
        bpt::key_t temp_key(temp_str.size(), temp_str.data());
        bpt->insert(temp_key, i);
        // if (i % 1000 == 0) {
        //     std::cout << "data:" << i << std::endl;
        // }
        // std::cout << "insert" << temp_str  << std::endl;
        // sleep(1);
    }
    std::cout << "insert success" << std::endl;
    std::cout << "-----------------------------------------" << std::endl;
    std::cout << "-----------------------------------------" << std::endl;
    std::cout << "-----------------------------------------" << std::endl;
    std::cout << "-----------------------------------------" << std::endl;
    std::cout << "-----------------------------------------" << std::endl;
    std::cout << "-----------------------------------------" << std::endl;

    // int anwser = 0;
    // for (int i = 0; i < 10000; i++) {
    //     std::string temp_str (temp + std::to_string(i));
    //     bpt::key_t temp_key(temp_str.size(), temp_str.data());
    //     int ret = bpt->search(temp_key, &anwser);
    //     if (ret == 0) {
    //         // if (i % 1000 == 0) {
    //         //     std::cout << "search KEY" << temp_str << ": " << anser << std::endl;
    //         //     // std::cout << "data:" << i << std::endl;
    //         // }
    //         std::cout << "search" << temp_str << ":" << anwser << std::endl;
    //     } else {
    //         std::cout << "data error" << std::endl;
    //         abort();
    //     }
    //     // sleep(1);
        
    // }
    bpt::bplus_tree::iterator ite = bpt->ite_begin();
    int current_cnt = 0;
    std::set<int> s;

    while (ite != bpt->ite_end()) {
        auto [length, key, val] = *ite;
        // void *dptr = fixed_pool_alloc(int_global_fixed_pool);
        // *((int*)dptr) = val;
        int temp_val = val;
        for (int i = 0; i < length; i++) {
            printf("%c", key[i]);
        }
        printf(":%d", temp_val);
        
        printf("\n");
        current_cnt++;
        s.insert(val);
        // if (current_cnt % 1000 == 0) {
        //     std::cout << "1000个数据" << std::endl;
        //     // sleep(5);
        // } 
        // bkey_t temp_key = {0};
        // temp_key.data_ptrs = dptr;
        // temp_key.length = length;
        // sprintf(temp_key.key, "%s", key.data());
        // memcpy(temp_key.key, key.data(), temp_key.length);
        // btree_insert(global_m_btree, temp_key, int_global_fixed_pool);
        ++ite;
    }
    for (int i = 0; i < 10000; i++) {
        if (s.contains(i) == false ) {
            std::cout << i << "值不存在" << std::endl;
            abort();
        }
        current_cnt++;
        if (current_cnt % 1000 == 0) {
            std::cout << "1000个数据" << std::endl;
            sleep(5);
        } 
    }
    return 0;
}