#pragma once

/*
我这里是如果遇到了一个删除操作,那么在放入线程池之前,就去锁定某个变量,使得新的操作暂不执行,并且在前面的所有操作都执行完后,再执行删除操作,那么对于锁定的变量就需要一个机制判断什么时候解锁
这里用一个原子变量或者锁,来锁定当前remove操作之前的前一个操作,并且把这个标识符记录下来,删除操作这里会阻塞当前线程,即不去放入新的任务,然后直到当前对应序列号的操作执行完毕,那么再去执行删除操作
这里如果不是删除操作都是普通的插入操作,那么就不使用这个锁,但是还是统一判断,当前锁是否被锁住了
相当于是记录当前操作的上一个操作是否需要被阻塞,这里上一个操作执行完,那么其他操作必然执行完吗,也不是所以这里情况很复杂,或者说就是架构的问题
这里更好的架构是一个客户端对应一个任务线程,这样呢就是线性的任务处理,避免插入和删除任务不同步的弊端.那么这里能就先使用这个架构,并且只使用insert插入数据先实现一些基本功能
*/
#include <sys/mman.h>    // C 风格头文件
#include <fcntl.h>      // open(), O_APPEND等
#include <unistd.h>     // write(), close()
#include <sys/stat.h>   // mode_t, 权限常量
#include "../BPlusTree/bpt.hpp"
#include <atomic>
#include <shared_mutex>
#include <cstdio>
#include <cstring>
#include <cerrno>
#include <cstdint>
#include <thread>
#include <chrono>
#include <compare> 
namespace Persister {


class persister {
// 指向全量文件的指针
// 指向增量文件的指针
// 这里使用增量文件时再去刷盘
// 这里使用O_APPEND,使用open系统调用，然后在合成新全量时再去刷盘fsync
private:


    
    constexpr static int limit = 1024 * 64;
    // 当前增量文件大小记录
    std::atomic<long> incre_split_size_;
    // 创建文件的原子阻塞
    std::atomic<bool> create_bool_;
    std::atomic<uint32_t> sequence_;
    std::atomic<uint64_t> file_suffix_;

    // int _full_log;
    std::atomic<int> _incremental_log;
// 对于非创建文件的线程只需要下面的原子变量，这里可以用分页标志，优化性能    
    
// 当前处理任务计数
    std::atomic<int> proc_num_;
// 读写锁的原子标志 
    std::atomic<bool> lock_sign_;
    //读写锁, default 
    std::shared_mutex rwlock_;
    
    const std::string _full_path;
    
    std::atomic<bool> wc_sign_;
    std::atomic<int> w_num_;
    bpt::bplus_tree _btree;
    


    // 这里在程序开始时就去初始化该全量和增量文件
    // 如果不存在那么，赋值为-1
    void combine_flush_log();
    void quit_func();
    void cmd_quit_thread();
public:
    
    // void start_thread() noexcept;
    persister(const char* path);
    void start_combine_flush_thread() noexcept;
    // 接收key val , num
    void persiste(std::string const& key, int val, int num = 1) noexcept;
    void start_cmd_quit_thread() noexcept;
};

}
