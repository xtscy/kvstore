#include "bpt.hpp"
#include "predefined.hpp"
#include <iostream>
#include <string>

int main() {


    bpt::bplus_tree *bpt = new bpt::bplus_tree("./test1.db", false);
    std::string temp = "KEY";
    for (int i = 0; i < 10; i++) {
        std::string temp_str (temp + std::to_string(i));
        std::cout << temp_str << "len:" << temp_str.size() << std::endl;
        bpt::key_t temp_key(temp_str.size(), temp_str.data());
        bpt->insert(temp_key, i);
        if (i % 1000 == 0) {
            std::cout << "data:" << i << std::endl;
        }
        std::cout << "insert" << temp_str  << std::endl;
        sleep(1);
    }
   

    int anser = 0;
    for (int i = 0; i < 10; i++) {
        std::string temp_str (temp + std::to_string(i));
        bpt::key_t temp_key(temp_str.size(), temp_str.data());
        int ret = bpt->search(temp_key, &anser);
        if (ret == 0) {
        std::cout << "search KEY" << temp_str << ": " << anser << std::endl;
        } else {
            std::cout << "data error" << std::endl;
            abort();
        }
        std::cout << "search" << temp_str << std::endl;
        sleep(1);
        
    }
    return 0;
}