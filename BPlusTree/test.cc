#include "bpt.h"
#include "predefined.h"
#include <iostream>
#include <string>

int main() {


    bpt::bplus_tree *bpt = new bpt::bplus_tree("./test1.db", false);
    std::string temp = "KEY";
    for (int i = 0; i < 1000000; i++) {
        bpt->insert((temp + std::to_string(i)).c_str(), i);
        if (i % 1000 == 0) {
            std::cout << "data:" << i << std::endl;
        }
    }
   

    int anser = 0;
    std::string str = "KEY";
    for (int i = 0; i < 1000000; i++) {
        
        int ret = bpt->search(bpt::key_t((str + std::to_string(i)).c_str()), &anser);
        if (ret == 0) {
        std::cout << "search KEY" << i << ": " << anser << std::endl;
        } else {
            std::cout << "data error" << std::endl;
            exit(-10);
        }
    }
    return 0;
}