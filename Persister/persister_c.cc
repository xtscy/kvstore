#include "persister.hpp"
#include "persister_c.h"

extern "C" {

struct persister_wrapper {
    Persister::persister *persister;
};
    
persister_handle persister_create_c(const char *path) {
    persister_wrapper *wrapper = new persister_wrapper();
    wrapper->persister = new Persister::persister(path);
    // wrapper->persister->start_combine_flush_thread();
    return static_cast<persister_handle>(wrapper);    
}

// key,val
bool persister_insert(persister_handle handle,const char *p_key, int val) {
    persister_wrapper *wrapper = static_cast<persister_wrapper*>(handle);
    std::string key(p_key);
    wrapper->persister->persiste(key, val);
    return true;
}

bool persister_get(persister_handle handle, const char *p_key) {
    persister_wrapper *wrapper = static_cast<persister_wrapper*>(handle);
    std::string key(p_key);
    wrapper->persister->persiste(key, 0, 2);
    return true;
}


}