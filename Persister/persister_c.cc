#include "persister.hpp"
#include "persister_c.h"

extern "C" {

struct persister_wrapper {
    Persister::persister *persister;
};
    
persister_handle persister_create_c(const char *path) {
    persister_wrapper *wrapper = new persister_wrapper();
    wrapper->persister = new Persister::persister(path);
    // 这里后续优化让另外一个进程来刷新文件，因为耗时非常大
    // 刷新文件只涉及到已经存在的增量文件刷新到全量文件中,即给b+tree添加值
    // 这里任务处理并不会用到b+树而是写到内存和增量文件中所以这里并不需要阻塞
    // 所以可以不阻塞主线程的任务执行，然后刷新一定的数量比如10个不满足10个就刷新到当前增量文件的前1个
    // 这样的话就能够更加高效的处理任务
    // 目前先用quit来刷新，先这样写
    wrapper->persister->start_cmd_quit_thread();
    // wrapper->persister->start_combine_flush_thread();
    return static_cast<persister_handle>(wrapper);    
}

bool persister_insert(persister_handle handle, uint16_t len, const char *p_key, int val) {
    persister_wrapper *wrapper = static_cast<persister_wrapper*>(handle);
    std::string key;
    key.assign(p_key, len);
    wrapper->persister->persiste(key, val);
    return true;
}

bool persister_get(persister_handle handle, uint16_t len, const char *p_key) {
    persister_wrapper *wrapper = static_cast<persister_wrapper*>(handle);
    std::string key;
    key.assign(p_key, len);
    wrapper->persister->persiste(key, 0, 2);
    return true;
}

bool persister_quit(persister_handle handle)  {
    persister_wrapper *wrapper = static_cast<persister_wrapper*>(handle);
    wrapper->persister->quit_func();
    std::cout << "quit正常退出" << std::endl;
    exit(0);
    return true;
}

}