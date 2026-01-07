#include "persister.hpp"
extern "C" {
    #include "../MemoryBTreeLock/m_btree.h"
}

namespace Persister{

persister::persister(const char* path)
    : _full_path(path), _btree(_full_path.c_str(), false)
    , proc_num_(0), lock_sign_(false), sequence_(0)
    , file_suffix_(0), _incremental_log(0), wc_sign_(true), w_num_(0)
{

    // 如果全量文件存在
    // 那么读取该文件
    // 这里使用迭代器依次获取每个值
    // 这里由于是加载，只会存在一个线程
    // 那么这里全量日志可以直接读
    // 不会有竞争问题
// 在使用迭代器遍历的情况下
    // 那么肯定是当前全量文件
    // 不会有其他操作
    // 所以在迭代器内部直接用单线程逻辑就行
    bpt::bplus_tree::iterator ite = _btree.ite_begin();
    while (ite != _btree.ite_end()) {
        auto [length, key, val] = *ite;
        void *dptr = fixed_pool_alloc(int_global_fixed_pool);
        *((int*)dptr) = val;
        bkey_t temp_key = {0};
        temp_key.data_ptrs = dptr;
        temp_key.length = length;
        memcpy(temp_key.key, key.data(), temp_key.length);
        btree_insert(global_m_btree, &temp_key, int_global_fixed_pool);
        ++ite;
    }
    // bpt::bplus_tree::iterator ite = _btree->ite_begin();
    // while (ite != _btree->ite_end()) {
    //     auto [length, key, val] = *ite;
    //     void *dptr = fixed_pool_alloc(int_global_fixed_pool);
    //     *((int*)dptr) = val;
    //     bkey_t temp_key = {0};
    //     temp_key.data_ptrs = dptr;
    //     temp_key.length = length;
    //     sprintf(temp_key.key, "%s", key.data());
    //     memcpy(temp_key.key, key.data(), temp_key.length);
    //     btree_insert(global_m_btree, temp_key, int_global_fixed_pool);
    //     ++ite;
    // }
    // 数据 同步完成

    // 再去读增量日志,如果有的话
    // 这里需要解析协议
    // 1 key1 val1\r\n  解析这个字符串，遇到\r\n说明是下一个命令，1代表set2代表get
    // 同步时只执行修改操作即set或者remove,get跳过，因为没有影响
    // 增量日志文件名为incremental_1,incremental_2,incremental_3 ...
    std::string prex_filename("incremental_");
    int i = 0;  
    FILE *fp = nullptr; 
    int fd = -1;
    // 使用映射文件
    std::string filename;
    std::string prev_filename("");
    while (true) {
        i++;
        filename = prex_filename + std::to_string(i) + ".log";
        // 这里需要先打开文件，同时构建文件名
        // 如果当前文件打开失败，说明没有增量日志，日志已经处理完毕
        fp = fopen(filename.c_str(), "rb");

        if (fp == nullptr) break;

        fd = ::fileno(fp);
        struct stat st;
        if (::fstat(fd, &st) < 0) {
            ::fclose(fp);
            throw std::runtime_error("无法获取文件大小");
        }
        size_t size = static_cast<size_t>(st.st_size);
        char *addr = nullptr;
        if (size > 0) {
            // 创建内存映射
            addr = (char*)::mmap(nullptr, size, PROT_READ, MAP_PRIVATE, fd, 0);
            if (addr == MAP_FAILED) {
                ::fclose(fp);
                throw std::runtime_error("内存映射失败");
            }

            // 对字符串进行解析
            //1 key1 val1\r\n
            int current = 0;
            std::string key;
            std::string val;
            std::string length;
            while (current < size) {

                char c = addr[current++];
                if (current == size) {
                    throw std::runtime_error("日志损坏");
                }

                key.clear();
                val.clear();

                if (c == '1') {
                    // set
                    while (current < size && addr[current] == ' ') {
                        current++;
                    }
                    if (current == size) {
                        fclose(fp);
                        throw std::runtime_error("日志损坏");
                    }

                    // len
                    while (current < size && addr[current] != ' ') {
                        length += addr[current++];
                    }

                    if (current == size) {
                        fclose(fp);
                        throw std::runtime_error("日志损坏");
                    }

                    // while (current < size && addr[current] == ' ') {
                    //     current++;
                    // }
                    current++;
                    
                    if (current == size) {
                        fclose(fp);
                        throw std::runtime_error("日志损坏");
                    }

                    //key
                    int len = 0;
                    auto [ptr1, ec1] = std::from_chars(length.data(), 
                        length.data() + length.size(),
                        len);
                    // if (ec == std::errc()) {
                        // std::cout << "转换成功: " << value << std::endl;
                        // std::cout << "已处理字符数: " << (ptr - str.data()) << std::endl;
                    if (ec1 == std::errc::invalid_argument) {
                        std::cerr << "无效参数" << std::endl;
                        throw std::runtime_error("键的长度无效 无效参数");
                    } else if (ec1 == std::errc::result_out_of_range) {
                        std::cerr << "超出范围" << std::endl;
                        throw std::runtime_error("键的长度无效 超出范围");
                    }
                    int temp_len = len;
                    while (current < size && temp_len > 0) {
                        key += addr[current++];
                        temp_len--;
                    }
                    
                    if (current == size) {
                        /**
                         * 1 4 key1 1
                        1 4 key2 2
                        1 4 key3 3
                        1 5 key11 11

                         */
                        fclose(fp);
                        throw std::runtime_error("日志损坏");
                    }
                    // 1 key1_len key1 val1\r\n
                    // //1 key1 val1\r\n2 key1\r\n
                    // skip 
                    while (current < size && addr[current] == ' ') {
                        current++;
                    }
                    if (current == size) {
                        fclose(fp);
                        throw std::runtime_error("日志损坏");
                    }
                    // char prev = '';
                    //val
                    while (current < size) {
                        if (addr[current] == '\r' && current + 1 < size && addr[current + 1] == '\n') {
                            current += 2;
                            break;
                        }
                        // else {
                        //     fclose(fp);
                        //     /*
                        //         1 4 key1 1
                        //         1 4 key2 2
                        //         1 4 key3 3
                        //         1 5 key11 11
                        //     */
                        //     throw std::runtime_error("日志损坏");
                        // }

                        if (current + 1 == size) {
                            fclose(fp);
                            throw std::runtime_error("日志损坏");
                        }
                        
                        val += addr[current++];
                    }
                    int c_val = 0;
                    auto [ptr, ec] = std::from_chars(val.data(),
                        val.data() + val.size(),
                        c_val);
                    // key val
                    // insert到全量文件
                    // _btree.insert(bpt::key_t(key.c_str()), std::stoi(val));
                    // _btree.insert(bpt::key_t(len, key.data()), c_val);

                    void *dptr = fixed_pool_alloc(int_global_fixed_pool);
                    *((int*)dptr) = c_val;
                    bkey_t temp_key = {0};
                    temp_key.data_ptrs = dptr;
                    temp_key.length = len;
                    memcpy(temp_key.key, key.data(), temp_key.length);
                    btree_insert(global_m_btree, &temp_key, int_global_fixed_pool);
                } else if (c == '2') {
                    // get
                    // 2 key1_len key1\r\n
                    std::string length;
                    while (current < size && addr[current] == ' ') {
                        current++;
                    }
                    if (current == size) {
                        fclose(fp);
                        throw std::runtime_error("日志损坏");
                    }
                    
                    // 读长度
                    while (current < size && addr[current] != ' ') {
                        length += addr[current++];
                    }
                    
                    if (current == size) {
                        fclose(fp);
                        throw std::runtime_error("日志损坏");
                    }
                    int len = 0;
                    auto [ptr, ec] = std::from_chars(length.data(), 
                        length.data() + length.size(),
                        len);
                    if (ec == std::errc::invalid_argument) {
                        std::cerr << "无效参数" << std::endl;
                        throw std::runtime_error("键的长度无效 无效参数");
                    } else if (ec == std::errc::result_out_of_range) {
                        std::cerr << "超出范围" << std::endl;
                        throw std::runtime_error("键的长度无效 超出范围");
                    }
                    current++;
                    current += len;
                    current += 2;
                    // char prev = '\0';
                    // while (current < size) {
                    //     if (addr[current] == '\n' && prev == '\r') {
                    //         current++;
                    //         break;
                    //     }
                    //     prev = addr[current++];
                    // }
                } else if (c == '3') {
                    //remove
                }
                length.clear();
            }   
            if (munmap(addr, size) == -1) {
                perror("munmap");
                throw std::runtime_error("munmap failed");
            }
        }
        prev_filename = filename;
        fclose(fp);
    }
    //  全量和增量遍历完
    // 这里先fclose，然后再用fd打开，如果一个文件都没有那么就创建文件然后再指向
    std::string create_file("");
    int create_sign = 0;
    if (prev_filename == "") {
        // 创建新文件
        create_sign = 1;
        create_file = "incremental_1.log";
        file_suffix_.store(2, std::memory_order_release);
    } else {
        // 创建新的增量文件,并指向
        // 文件名不为空，则打开了至少一个文件，prev_filename指向最后一个正常打开的文件
        // 然后去判断当前文件大小是否大于阈值，如果大于阈值那么依然重新创建文件
        int temp = open(prev_filename.c_str(), O_RDWR | O_APPEND | O_CREAT, 0644);
        if (temp == -1) {
            throw std::runtime_error("文件open出错");
        }
        struct stat st;
        if (::fstat(temp, &st) < 0) {
            ::close(temp);
            throw std::runtime_error("无法获取文件大小");
        }
        size_t size = static_cast<size_t>(st.st_size);
        if (size >= this->limit - 256) {
            create_sign = 1;
            create_file = std::string("incremental_") + std::to_string(i) + ".log";
            file_suffix_.store(i + 1, std::memory_order_relaxed);
            close(temp);
        } else {
            file_suffix_.store(i, std::memory_order_relaxed);
            _incremental_log.store(temp, std::memory_order_relaxed);
        }
    }

    if (create_sign == 1) {
        int temp = open(create_file.c_str(), O_RDWR | O_APPEND | O_CREAT, 0644);
        if (temp == -1) {
            throw std::runtime_error("文件open出错");
        }
        _incremental_log.store(temp, std::memory_order_relaxed);
    }
    
    struct stat st;
    if (::fstat(_incremental_log, &st) < 0) {
        ::close(_incremental_log.load());
        throw std::runtime_error("无法获取文件大小");
    }

    incre_split_size_.store(st.st_size, std::memory_order_release);
    // _incremental_log指向最新的存放增量日志的文件
    // 设置当前
}

/*
num 为当前写入的操作的数字标识
*/
// 这里的persiste也需要更改
void persister::persiste(std::string const& key, int val, int num) noexcept {

    // 使用2个原子变量 ，1个读写锁
    // 大小大于分割值，分割文件
    bool create = false;
    uint32_t seq_o = 0;
    uint32_t seq_i = 0;
    //* 这里可能有多个线程同时进入大于limit,只有一个线程能够创建，其他线程阻塞
    //? 这里用if则是粗略的只是跳过当前文件，对于后面的文件，当我创建了新文件
    //? 直接就去写入，但是这里的写入可能新文件写入速度很快
    //? 有其他线程写入，那么对于新的文件在当前线程还没写入的时候
    //? 新文件又被写满了，那么这个写入速度必须非常快
    //? 因为当我切换标志后，阻塞线程就会出去，直接执行写入操作
    //? 只会有有一个原子的load 100纳秒和while 1纳秒的条件判断然后去写入操作
    //? 内存1微秒写入50字节，SSD 10字节，HDD 0.1字节
    //? 在这个极短的100纳秒时间里其他线程不可能写满1024或者更多
    //? 所以在操作完成，在当前的考虑时间内，不可能把1个文件写满1024或以上
    //? 线程的时间片是毫秒单位50毫秒，write不刷就是1-2微秒，刷的话就是1-10毫秒
    

    //? 这里是写入后才会增加统计计数
    //? 所以有一批线程是在临近阈值进行写入
    //? 这里对于写入超过阈值的线程还没进行写入write
    //? 然后新的文件创建了并且更新了_incremental_log
    //? 那么原本是写入超过阈值的文件，转而写入了新的创建文件
    //? 那么这个其实也起到了缓解写入阈值文件的情况
    //? 尽可能不超过阈值

    //1 key1 val1\r\n2 key1\r\n
    /*
        std::string original = "hello";
    int num = 42;
    int val = 100;
    
    ?C++20 的 std::format（需要编译器支持C++20）
    std::string result = std::format("{}{}{}", num, original, val);
    std::cout << "结果: " << result << std::endl;  // 输出: 42hello100
    */
   /*
    std::array<char, 20> buffer;  // 足够容纳任何整数的字符串表示
    auto [ptr, ec] = std::to_chars(buffer.data(), buffer.data() + buffer.size(), value);
   */
  //这里字符串拼接可以优化，目前先这样写
    std::string data;
    // 这里 添加长度前缀
    if (num == 1) {
        data = std::to_string(num) + " " + std::to_string(key.size()) + " " + key + " " + std::to_string(val) + "\r\n";
    } else if (num == 2) {
        data = std::to_string(num) + " " + std::to_string(key.size()) + " " + key + "\r\n";
    }
    //? 还是使用while,而非if，尽可能的保证不向到达阈值的文件进行写入
    int loop = 0;
    
    // 这里每个分支都使用了break，那么当前的loop是无用的，这里先这样，后续看要不要去掉某个break
    while (incre_split_size_.load(std::memory_order_acquire) >= this->limit) {
        if (loop == 10) break;//*防止一直阻塞
        // seq
        //线程停留
        seq_o = sequence_.load(std::memory_order_relaxed);
        if (create_bool_.compare_exchange_strong(create, true,std::memory_order_acquire, std::memory_order_relaxed)) {
            // seq 
            seq_i = sequence_.load(std::memory_order_relaxed);
            // 这里进来则序列号一定更改，在标志设置更新前去设置序列号
            if (seq_i == seq_o) {
                int lock_temp = 0;
                proc_num_.fetch_add(1, std::memory_order_relaxed);
                if (lock_sign_.load(std::memory_order_relaxed) == true) {
                    lock_temp = 1;
                    printf("lock_shared behind\n");
                    rwlock_.lock_shared();
                    printf("lock_shared after\n");
                }
                
                // 比较成功，则去创建
                // 这里再检查一次大小，如果大小大于等于limit那么进行真正的创建
                if (incre_split_size_.load(std::memory_order_acquire) >= this->limit) {
                    uint64_t suffix = file_suffix_.fetch_add(1, std::memory_order_relaxed);
                    std::string filename = "incremental_" + std::to_string(suffix) + ".log";
                    int new_fd = open(filename.c_str(), O_RDWR | O_APPEND | O_CREAT, 0644);
                    if (new_fd == -1) {
                        throw std::runtime_error("文件open出错");
                    }
                    //* 成功创建，按照顺序来更新原子值
                    //todo 这里涉及下面可能还有写入操作，当前的 incre_split_size_的更新
                    //todo _incremental_log fd的更新
                    //todo 以及最后的create_bool_的更新,还有当前fd的关闭
                    //todo 考虑到下面的对于当前fd的写入操作
                    //todo 如果这里先关闭，那么可能写入到错误的fd中
                    //todo 所以关闭一定在fd的更新后面，即先记录当前的fd然后更新fd再去关闭
                    //todo 那么这里的_size_该如何更新呢，在更新了fd后再去更新
                    //todo 那么这样的话就可能下面的写入操作在_size_更新的前面
                    //todo 然后向该fd前面写入的一些数据就会未记录,所以会少记录一些数据
                    //todo 由于每次写入  的数据并不是非常大，如果每次写入的数据非常大
                    //todo 那么在下面的写入操作需要再增加一些检测逻辑,这里数据小所以浮动范围可以接收
                    //todo 那么就先修改fd，然后更新size最后关闭原fd
                    //todo 这里呢下面写入操作还是可能失败因为原子加载和写入两个操作合在一起，并不是原子的
                    //todo 下面写入的时候使用while直到写入成功

                    wc_sign_.store(false, std::memory_order_relaxed);// 这里锁住其他写操作
                    // 这里只有当w_num_为0时才会去更新当前的文件信息
                    std::cout << "320 if (w_num_.load(std::memory_order_relaxed) == 0)" << std::endl;
                    while (true) {
                        if (w_num_.load(std::memory_order_relaxed) == 0) {
                            std::cout << "323 if (w_num_.load(std::memory_order_relaxed) == 0)" << std::endl;

                            int old = _incremental_log.exchange(new_fd, std::memory_order_relaxed);

                            ::fsync(old);// 后续可以优化

                            close(old);
                            // create_bool_, incre_split_size_, wc_sign_
                            // create_bool_控制还未CAS进入的线程和在下面while循环的线程
                            // incre_split_size_控制最外层的还未进入while的线程
                            // 外层while不进入则直接写入，如果wc_sign_没更新那么将会阻塞在
                            // 同一个位置，这里为了降低线程阻塞在同一个区域
                            // 那么在更新incre_split_size_前更新wc_sign_
                            // 但是这样的话incre_split_size可能会少记录在下面while
                            // 的线程写入的字节数，然后被incre_split_size_更新为0
                            // 但是在上面原本要下去阻塞的线程这时阻塞到了下面的while
                            // 不，这里是不同的堆栈所以更新了信息后，尽可能的在下面阻塞
                            // 可是这里涉及到共同访问一个缓存行即wc_sign_
                            // 那么其实可能并不是并发，这里每个线程内部都需要去更新这个
                            // wc_sign_的值,这里缓存行被标明为失效然后都去获取
                            // 那么其实会有更大的竞争，那么这里呢就先更新wc_sign_
                            // 然后更新incre_split_size_
                            wc_sign_.store(true, std::memory_order_relaxed);//释放下面阻塞线程
                            sequence_.fetch_add(1, std::memory_order_relaxed);// 更新序列号，让进入的线程
                            incre_split_size_.store(0, std::memory_order_relaxed);// 让上面的线程去到write部分
                            create_bool_.store(false, std::memory_order_relaxed);
                            // 最后释放当前CAS失败的线程
                            // 先减1再解锁?
                            std::cout << "if (lock_temp) rwlock_.unlock_shared();" << std::endl;
                            if (lock_temp) rwlock_.unlock_shared();
                            proc_num_.fetch_sub(1, std::memory_order_relaxed);
                            break;
                        }
                    }  
                    break; //创建成功直接break
                } else {
                    create_bool_.store(false, std::memory_order_relaxed);
                    std::cout << "rwlock_.unlock_shared behind" << std::endl;
                    if (lock_temp) rwlock_.unlock_shared();
                    std::cout << "rwlock_.unlock_shared after" << std::endl;
                    proc_num_.fetch_sub(1, std::memory_order_relaxed);
                    break;// 当前空间充足,这里处理极端的情况在上面停留的线程，等到了新的文件创建完成才去CAS
                }
                
            } else {
                // 序列号不同，说明已经创建了新的文件,这里刷新标志
                // 然后退出这里的while创建逻辑，直接去写入数据
                // 虽然这里直接去写入可能写入的当前文件达到阈值，新文件还没有创建完成
                //* 这里序列号不同说明已经是上一批上上批的操作了
                //* 这里直接去写入就算到达阈值，避免该操作阻塞太久
                create_bool_.store(false, std::memory_order_relaxed);
                // if (lock_temp) rwlock_.unlock_shared();
                break;
            }
            //? 不等于则不创建，说明当前批已经有线程去创建新的文件了
            //? 这里退出if逻辑去进行数据的追加
            //? 这里有一种可能就是有多个线程运行不及时
            //? 然后一个当前文件可能达到了阈值，然后继续向该文件进行写入
            //? 这里如果线程写入的数据足够大，并且线程数量足够多
            //? 那么某个文件可能非常之大
            //? 但是这是很难出现的 因为首先要在两个if之间停留，同时停留的线程数量还必须非常大
            
        } else {
            // 比较失败,create值被更改为true
            while (create_bool_.load(std::memory_order_relaxed) == true) {}
            // 循环直到为false
            // 这里可以直接break
            // 也可以loop++后到上面再执行1个原子操作
            // 可是能走到这必然说明等待的任务创建已经完成，然后可以进行写入数据，create_bool_已经被置为false
            // 这里可以直接break;
            break;
        }
        loop++;
    }

    // 把val转成string， 然后把key和val按照格式写入增量文件
    std::string value(std::to_string(val));

    // 这里呢，对文件进行写入
    // 先判断wc_sign_是否为true,如果为true那么去执行写操作，false说明当前文件正在创建不能写入，因为可能调用close，并且这里还同步了其他变量
    // 这里先增加写计数，然后去判断标志，如果标志 is false，那么回退减少当前的写计数，循环等待
    // 直到标志为true那么才继续去尝试写入，这样在创建的时候判断w_num_时不会死锁
    // 这里直到当前标志is true时再去进行写入操作，++w_num_
    // 所以这里有一个回退操作，那么如何去编程实现呢

    //todo 1. 这里先写完写入逻辑
    //todo 2. 然后写一个用于把增量合成全量的工作线程的刷新逻辑, 写到while外层

    int lk_temp = 0;
    proc_num_.fetch_add(1, std::memory_order_relaxed);
    if (lock_sign_.load(std::memory_order_relaxed) == true) {
        lk_temp = 1;
        std::cout << "410 rwlock_.lock_shared behind" << std::endl;
        rwlock_.lock_shared();
        std::cout << "410 rwlock_.lock_shared after" << std::endl;
    }
    // 先++w_num_
    while (true) {
        // 已经锁定那么回退，等待
        // if (wc_sign_.load(std::memory_order_relaxed) == false) {
            //     w_num_.fetch_sub(1, std::memory_order_relaxed);
            // } else { // 没有锁定
            // }
        // 这里的优先级为当前线程如果判断了没有锁定，那么先执行该写入操作
        // 虽然可能wc_sign_在判断之后被设为了false,创建操作进行一个阻塞直到w_num_为0
        // 当这里wc_sign_还未锁定得到true时那边必然还没有判断w_num_
        // 使得这里如果还在写入没有sub,那么那边的创建操作必然阻塞
        // 而如果那边先进入锁定了wc_sign_,那么这里必然阻塞回退然会循环等待标志为true
        // 
        w_num_.fetch_add(1, std::memory_order_relaxed);
        if (wc_sign_.load(std::memory_order_relaxed) == true) { // 没有锁定
            // 去写入
            ssize_t result = write(_incremental_log.load(std::memory_order_relaxed), 
                                data.data(), data.size());
            if (result == -1 || result < data.size()) {
                // 写入失败
                perror("write failed");
                throw std::runtime_error(
                    "错误码: " + std::to_string(errno) + 
                    ", 描述: " + std::strerror(errno)
                );
            }

            //成功写入，累加incre_split_size_
            incre_split_size_.fetch_add(result, std::memory_order_relaxed);
            w_num_.fetch_sub(1, std::memory_order_relaxed);
            //成功写入后直接退出当前操作，这里可以用return
            
            if (lk_temp) rwlock_.unlock_shared();
            proc_num_.fetch_sub(1, std::memory_order_relaxed);
            return;
        } else { // 锁定
            w_num_.fetch_sub(1, std::memory_order_relaxed);
            while (wc_sign_.load(std::memory_order_relaxed) != true) {}
        }
    }
}


// void persister::combine_flush_log() {

    // uint16_t num = 0;
    
    // while(true) {
//         num++;
//         if (num % 3 != 0) {
//             std::chrono::steady_clock::time_point now = std::chrono::steady_clock::now();
//             auto target_time = now + std::chrono::seconds(60);
//             std::this_thread::sleep_until(target_time);
//             ::fsync();


            
//             // 先锁定write
//             // 这里当我合并的时候肯定不能写
//             // 当我写的时候肯定不能合并。因为合并会修改文件，写也会修改文件
//             // 那么这里肯定需要判断当前是否有任务正在进行
//             // 原本的想法是直接使用读写锁，那么为了优化
//             // 在写的时候的无意义的加解锁，那么使用了原子值
//             // 这里可以先判断原子任务数是否为0,为0再去, 获取读写锁
//             // 这里是否有并发问题呢
//             // 这里普通的就是加锁然后解锁
//             // 使用原子变量就是先检测是否原子加锁  
//             // 阻塞新的写入，然后旧的写入继续写入
//             // 在合并线程中设置了lock_sign_后
//             // 也需要先去检测当前原子计数是否为0
//             // 这里先获取写锁，因为现在没有任何的线程获取锁
//             // 那里的写入操作也是在读到lock_sign_为true之后才去获取写锁
//             // 这里获取写锁后再去锁定原子标志
//             //  然后在内部判断proc_num_是否为0，为0后再去进行合并操作
//             // 不为0的话这里是不能阻塞的，这里是让写入线程进行阻塞睡眠
//             // 因为fsync的耗时是write的几千倍 


//             // 那么这里相当于是我要合并前肯定要调用fsync
//             // 那么是在锁定写锁前调用还是写锁后呢，
//             // 这里肯定是当write写任务全部执行完才去调用fsync
//             // 否则可能没有同步完数据到磁盘，没有把全部操作写到全量
//             // 只把刷新的写到全量后，就把该增量文件删除了

            
            
            
//             // 合并
//             // 这里rwlock + atomic
//             // 由于这里的fsycn()很耗时持久化到磁盘，所以直接去rwlock的sleep
//             // 这里的fsync同步的是前面所有的write调用
//             // write调用的写入在合并全量的时候全部写入磁盘

//         } else {
//             std::chrono::steady_clock::time_point now = std::chrono::steady_clock::now();
//             auto target_time = now + std::chrono::seconds(60);
//             std::this_thread::sleep_until(target_time);
//             ::fsync();
//         }
//     }
// }


void persister::combine_flush_log() {

    FILE *fp = NULL;
    int fd = -1;
    while (true) {
        std::chrono::steady_clock::time_point now = std::chrono::steady_clock::now();
        auto target_time = now + std::chrono::seconds(300);
        std::this_thread::sleep_until(target_time);
        
        // 这里直接combine, 在close的时候调用fsync
        std::string prex_filename("incremental_");
        lock_sign_.store(true, std::memory_order_relaxed);
        rwlock_.lock();
        while (true) {
            if (proc_num_.load(std::memory_order_relaxed) == 0) {
                int suffix = file_suffix_.load(std::memory_order_relaxed);
                ::fsync(_incremental_log.load(std::memory_order_relaxed));
                for (int i = 1; i < suffix; i++) {
                    // read all incremental log to full log
                    // call bpt insert
                    // read according to the format
                    std::string filename = prex_filename + std::to_string(i) + + ".log";
                    fp = fopen(filename.c_str(), "rb");

                    if (fp == nullptr) {
                        throw std::runtime_error("无法打开文件在combine时");
                    }
                    
                    
                    fd = ::fileno(fp);
                    struct stat st;
                    if (::fstat(fd, &st) < 0) {
                        ::fclose(fp);
                        throw std::runtime_error("无法获取文件大小");
                    }
                    size_t size = static_cast<size_t>(st.st_size);
                    char *addr = nullptr;
                    if (size > 0) {
                        // 创建内存映射
                        addr = (char*)::mmap(nullptr, size, PROT_READ, MAP_PRIVATE, fd, 0);
                        if (addr == MAP_FAILED) {
                            ::fclose(fp);
                            throw std::runtime_error("内存映射失败");
                        }

                        // 对字符串进行解析
                        //1 4 key1 val1\r\n
                        //   // 1 key1 val1\r\n
                        int current = 0;
                        std::string key;
                        std::string val;
                        std::string length;
                        while (current < size) {

                            char c = addr[current++];
                            if (current == size) {
                                throw std::runtime_error("日志损坏");
                            }

                            key.clear();
                            val.clear();

                            if (c == '1') {
                                // set
                                while ((current < size) && (addr[current] == ' ')) {
                                    current++;
                                }
                                if (current == size) {
                                    fclose(fp);
                                    throw std::runtime_error("日志损坏");
                                }

                                // len
                                while ((current < size) && (addr[current] != ' ')) {
                                    length += addr[current++];
                                }

                                if (current == size) {
                                    fclose(fp);
                                    throw std::runtime_error("日志损坏");
                                }

                                // while (current < size && addr[current] == ' ') {
                                //     current++;
                                // }
                                current++;
                                
                                if (current == size) {
                                    fclose(fp);
                                    throw std::runtime_error("日志损坏");
                                }

                                //key
                                int len = 0;
                                auto [ptr, ec] = std::from_chars(length.data(), 
                                    length.data() + length.size(),
                                    len);
                                // if (ec == std::errc()) {
                                    // std::cout << "转换成功: " << value << std::endl;
                                    // std::cout << "已处理字符数: " << (ptr - str.data()) << std::endl;
                                if (ec == std::errc::invalid_argument) {
                                    std::cerr << "无效参数" << std::endl;
                                    throw std::runtime_error("键的长度无效 无效参数");
                                } else if (ec == std::errc::result_out_of_range) {
                                    std::cerr << "超出范围" << std::endl;
                                    throw std::runtime_error("键的长度无效 超出范围");
                                }
                                int temp_len = len;
                                while (current < size && temp_len > 0) {
                                    key += addr[current++];
                                    temp_len--;
                                }
                                
                                if (current == size) {
                                    fclose(fp);
                                    throw std::runtime_error("日志损坏");
                                }
                                // 1 key1_len key1 val1\r\n
                                // //1 key1 val1\r\n2 key1\r\n
                                // skip 
                                while (current < size && addr[current] == ' ') {
                                    current++;
                                }
                                if (current == size) {
                                    fclose(fp);
                                    throw std::runtime_error("日志损坏");
                                }
                                // char prev = '';
                                //val
                                while (current < size) {
                                    if (addr[current] == '\r' && current + 1 < size && addr[current + 1] == '\n') {
                                        current += 2;
                                        break;
                                    } else {
                                        fclose(fp);
                                        throw std::runtime_error("日志损坏");
                                    }

                                    if (current + 1 == size) {
                                        fclose(fp);
                                        throw std::runtime_error("日志损坏");
                                    }
                                    
                                    val += addr[current++];
                                }
                                int c_val = 0;
                                auto [ptr1, ec1] = std::from_chars(val.data(),
                                    val.data() + val.size(),
                                    c_val);
                                // key val
                                // insert到全量文件
                                // _btree.insert(bpt::key_t(key.c_str()), std::stoi(val));
                                _btree.insert(bpt::key_t(len, key.data()), c_val);
                                
                            } else if (c == '2') {
                                // get
                                // 2 key1_len key1\r\n
                                std::string length;
                                while (current < size && addr[current] == ' ') {
                                    current++;
                                }
                                if (current == size) {
                                    fclose(fp);
                                    throw std::runtime_error("日志损坏");
                                }
                                
                                // 读长度
                                while (current < size && addr[current] != ' ') {
                                    length += addr[current++];
                                }
                                
                                if (current == size) {
                                    fclose(fp);
                                    throw std::runtime_error("日志损坏");
                                }
                                int len = 0;
                                auto [ptr, ec] = std::from_chars(length.data(), 
                                    length.data() + length.size(),
                                    len);
                                if (ec == std::errc::invalid_argument) {
                                    std::cerr << "无效参数" << std::endl;
                                    throw std::runtime_error("键的长度无效 无效参数");
                                } else if (ec == std::errc::result_out_of_range) {
                                    std::cerr << "超出范围" << std::endl;
                                    throw std::runtime_error("键的长度无效 超出范围");
                                }
                                current++;
                                current += len;
                                current += 2;
                                // char prev = '\0';
                                // while (current < size) {
                                //     if (addr[current] == '\n' && prev == '\r') {
                                //         current++;
                                //         break;
                                //     }
                                //     prev = addr[current++];
                                // }
                            } else if (c == '3') {
                                //remove
                            }
                        }   
                        if (munmap(addr, size) == -1) {
                            perror("munmap");
                            throw std::runtime_error("munmap except");
                        }
                    }
                    // 删除fp文件
                    fclose(fp);
                    ::remove(filename.c_str());       // 使用C的remove
                }
                // 删除所有的增量文件，重置信息
                // 重新创建文件, 更新_log指向，更新file_suffix_
                // 更新split_size_大小
                int log = open("incremental_1.log", O_RDWR | O_APPEND | O_CREAT, 0644);
                _incremental_log.store(log, std::memory_order_relaxed);
                file_suffix_.store(2, std::memory_order_relaxed);
                incre_split_size_.store(0, std::memory_order_relaxed);

                
                lock_sign_.store(false, std::memory_order_relaxed);
                rwlock_.unlock();
                break;
            }
        }
    }
}
void persister::start_combine_flush_thread() noexcept {

    // start and detach
    std::thread combine_t([this] {
        this->combine_flush_log();
    });
    combine_t.detach();
    std::cout << "combine thread start->>" << std::endl;
}

void persister::quit_func() {

    FILE *fp = NULL;
    int fd = -1;
    std::cout << "开始清理增量文件" << std::endl;
    std::string prex_filename("incremental_");
    lock_sign_.store(true, std::memory_order_relaxed);
    rwlock_.lock();
    if (proc_num_.load(std::memory_order_relaxed) == 0) {
        int suffix = file_suffix_.load(std::memory_order_relaxed);
        ::fsync(_incremental_log.load(std::memory_order_relaxed));
        for (int i = 1; i < suffix; i++) {
            // read all incremental log to full log
            // call bpt insert
            // read according to the format
            std::string filename = prex_filename + std::to_string(i) + + ".log";
            fp = fopen(filename.c_str(), "rb");

            if (fp == nullptr) {
                throw std::runtime_error("无法打开文件在combine时");
            }
            
            
            fd = ::fileno(fp);
            struct stat st;
            if (::fstat(fd, &st) < 0) {
                ::fclose(fp);
                throw std::runtime_error("无法获取文件大小");
            }
            size_t size = static_cast<size_t>(st.st_size);
            char *addr = nullptr;
            if (size > 0) {
                // 创建内存映射
                addr = (char*)::mmap(nullptr, size, PROT_READ, MAP_PRIVATE, fd, 0);
                if (addr == MAP_FAILED) {
                    ::fclose(fp);
                    throw std::runtime_error("内存映射失败");
                }

                // 对字符串进行解析
                //1 key1 val1\r\n
                int current = 0;
                std::string key;
                std::string val;
                std::string length;
            while (current < size) {

                char c = addr[current++];
                if (current == size) {
                    throw std::runtime_error("日志损坏");
                }

                key.clear();
                val.clear();
                length.clear();

                if (c == '1') {
                    // set
                    while (current < size && addr[current] == ' ') {
                        current++;
                    }
                    if (current == size) {
                        fclose(fp);
                        throw std::runtime_error("日志损坏");
                    }

                    // len
                    while ((current < size) && (addr[current] != ' ')) {
                        length += addr[current++];
                    }

                    if (current == size) {
                        fclose(fp);
                        throw std::runtime_error("日志损坏");
                    }

                    // while (current < size && addr[current] == ' ') {
                    //     current++;
                    // }
                    current++;
                    
                    if (current == size) {
                        fclose(fp);
                        throw std::runtime_error("日志损坏");
                    }

                    //key
                    int len = 0;
                    auto [ptr, ec] = std::from_chars(length.data(), 
                        length.data() + length.size(),
                        len);
                    // if (ec == std::errc()) {
                        // std::cout << "转换成功: " << value << std::endl;
                        // std::cout << "已处理字符数: " << (ptr - str.data()) << std::endl;
                    if (ec == std::errc::invalid_argument) {
                        std::cerr << "无效参数" << std::endl;
                        throw std::runtime_error("键的长度无效 无效参数");
                    } else if (ec == std::errc::result_out_of_range) {
                        std::cerr << "超出范围" << std::endl;
                        throw std::runtime_error("键的长度无效 超出范围");
                    }
                    int temp_len = len;
                    while (current < size && temp_len > 0) {
                        key += addr[current++];
                        temp_len--;
                    }
                    
                    if (current == size) {
                        fclose(fp);
                        throw std::runtime_error("日志损坏");
                    }
                    // 1 key1_len key1 val1\r\n
                    // //1 key1 val1\r\n2 key1\r\n
                    // skip 
                    while (current < size && addr[current] == ' ') {
                        current++;
                    }
                    if (current == size) {
                        fclose(fp);
                        throw std::runtime_error("日志损坏");
                    }
                    // char prev = '';
                    //val
                    while (current < size) {
                        if (addr[current] == '\r' && current + 1 < size && addr[current + 1] == '\n') {
                            current += 2;
                            break;
                        }
                        // } else {
                        //     fclose(fp);
                        //     throw std::runtime_error("日志损坏");
                        // }

                        if (current + 1 == size) {
                            fclose(fp);
                            throw std::runtime_error("日志损坏");
                        }
                        
                        val += addr[current++];
                    }
                    int c_val = 0;
                    auto [ptr1, ec1] = std::from_chars(val.data(),
                        val.data() + val.size(),
                        c_val);
                    // key val
                    // insert到全量文件
                    // _btree.insert(bpt::key_t(key.c_str()), std::stoi(val));
                    _btree.insert(bpt::key_t(len, key.data()), c_val);
                    
                } else if (c == '2') {
                    // get
                    // 2 key1_len key1\r\n
                    std::string length;
                    while (current < size && addr[current] == ' ') {
                        current++;
                    }
                    if (current == size) {
                        fclose(fp);
                        throw std::runtime_error("日志损坏");
                    }
                    
                    // 读长度
                    while (current < size && addr[current] != ' ') {
                        length += addr[current++];
                    }
                    
                    if (current == size) {
                        fclose(fp);
                        throw std::runtime_error("日志损坏");
                    }
                    int len = 0;
                    auto [ptr, ec] = std::from_chars(length.data(), 
                        length.data() + length.size(),
                        len);
                    if (ec == std::errc::invalid_argument) {
                        std::cerr << "无效参数" << std::endl;
                        throw std::runtime_error("键的长度无效 无效参数");
                    } else if (ec == std::errc::result_out_of_range) {
                        std::cerr << "超出范围" << std::endl;
                        throw std::runtime_error("键的长度无效 超出范围");
                    }
                    current++;
                    current += len;
                    current += 2;
                    // char prev = '\0';
                    // while (current < size) {
                    //     if (addr[current] == '\n' && prev == '\r') {
                    //         current++;
                    //         break;
                    //     }
                    //     prev = addr[current++];
                    // }
                } else if (c == '3') {
                    //remove
                }

            }   
                if (munmap(addr, size) == -1) {
                    perror("munmap");
                    throw std::runtime_error("munmap except");
                }
            }
            // 删除fp文件
            fclose(fp);
            ::remove(filename.c_str());       // 使用C的remove
        }
        // 删除所有的增量文件，重置信息
        // 重新创建文件, 更新_log指向，更新file_suffix_
        // 更新split_size_大小
        int log = open("incremental_1.log", O_RDWR | O_APPEND | O_CREAT, 0644);
        std::cout << "增量全部刷到全量，exit退出" << std::endl;
        exit(0);
        _incremental_log.store(log, std::memory_order_relaxed);
        file_suffix_.store(2, std::memory_order_relaxed);
        incre_split_size_.store(0, std::memory_order_relaxed);

        
        lock_sign_.store(false, std::memory_order_relaxed);
        rwlock_.unlock();
    }
        
}

void persister::start_cmd_quit_thread() noexcept {

    std::thread cmd_thread([this] {

        std::string input;
        std::string_view cmd("quit");
        while (true) {
            std::cout << "等待输入cmd" << std::endl;
            std::getline(std::cin, input);
            // std::cout << "输入cmd完成" << std::endl;
            auto result = input <=> cmd;
            if (result == std::strong_ordering::less) {

                std::cout << "cmd not exist\n请重新输入" << std::endl;
                
            } else if (result == std::strong_ordering::equal) {

                std::cout << "quit!" << std::endl;
                this->quit_func();
                exit(0);
                
            } else if (result == std::strong_ordering::greater) {
                std::cout << "cmd not exist\n请重新输入" << std::endl;
            }
        }
    });
    cmd_thread.detach();
}

}










