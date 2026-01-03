#ifndef RESP_STATE_STACK
#define RESP_STATE_STACK

#ifndef MAX_STACK_DEPTH
#define MAX_STACK_DEPTH 8  // 默认8层，覆盖99.9%场景
#endif

#ifndef MAX_LINE_BUFFER
#define MAX_LINE_BUFFER 1024  // 行缓冲区1KB
#endif
// ftype
typedef enum {
    // 通用状态
    STATE_INIT,             // 初始状态
    STATE_ARRAY_INIT,
    STATE_READING_TYPE,
    STATE_READING_LINE,     // 读取行数据
    STATE_EXPECTING_CR,     // 期望回车
    STATE_EXPECTING_ARRAY_LENGTH_LF,     // 期望换行
    STATE_READING_LINE_LF,
    
    // 批量字符串特定状态
    STATE_BULK_READING_LENGTH,  // 读取长度
    STATE_BULK_READING_LENGTH_LF,
    STATE_BULK_READING_DATA,    // 读取数据
    STATE_BULK_READING_DATA_LF, // 读取\n
    // 数组特定状态  
    STATE_ARRAY_READING_ELEMENTS_INIT,
    STATE_ARRAY_READING_LENGTH,   // 读取数组长度
    STATE_ARRAY_READING_ELEMENTS, // 读取数组元素
    // STATE_ARRAY_WAITING_ELEMENT,  // 等待下一个元素开始
    
    // 完成状态
    STATE_ARRAY_COMPLETE         // 解析完成
    // STATE_ERROR              // 解析错误

    // READ_ARRAY,
    // READ_TYPE,
    // READ_LENGTH,// $有该状态
    // READ_END// 读取实际数据
} read_state_t;
//state
typedef enum  {
    FRAME_SIMPLE,      // 简单类型（+ - :） - 需要等待CRLF
    FRAME_INT,
    FRAME_SIMPLE_CHAR,
    FRAME_BULK,        // 批量字符串（$） - 需要长度+数据+CRLF
    FRAME_ARRAY,       // 数组（*） - 需要多个元素
    FRAME_ARRAY_ELEMENT, // 数组元素
    FRAME_NONE
} frame_type_t;

// 状态栈结构（固定大小）
typedef struct resp_state_stack_s {
    // 栈存储（固定数组）
//无嵌套
    struct StackFrame {
        // uint8_t type;           // 类型: '+' '-' ':' '$' '*'
        read_state_t state;      // 状态: 等待长度/数据/CRLF等
        frame_type_t ftype;    // 当前的帧类型
        uint16_t reserved;      // 对齐
        int32_t expected_len;   // 期望长度
        int32_t received_len;   // 已接收长度
    } frames;
    
    // 栈指针
    // int8_t top;                 // -1表示空栈，0-7表示有数据
    
// 这里解析出来的数据都放在了下面的array数组指向
// 所以这里的line_buffer就是一个全局缓冲区
// 缓冲当前的暂存数据
    struct {
        char line_buffer[MAX_LINE_BUFFER];   // 行数据缓冲区（固定大小）
        size_t line_pos;         // 当前位置
        size_t expected_end;// 期望结束位置（如果有长度的话）
    } line;

    // 共享行缓冲区（固定大小）
    // char line_buffer[MAX_LINE_BUFFER];
    // uint16_t line_pos;          // 当前行位置
    // uint16_t line_cr_pos;       // CR位置（用于CRLF检测）
    
    // 对于批量字符串（$）的数据累积
    struct {
        // 两种存储策略：
        union {
            char inline_data[128];  // 小数据：内联存储
            char* heap_data;        // 大数据：堆存储,这里使用stage分配
        } storage;
        // read_state_t state;
        // size_t allocated;   // 已分配大小（动态存储时使用）
        size_t filled;      // 已填充大小
        size_t expected;    // 期望总大小,即批量字符串的长度
        uint8_t storage_type; // 0=内联, 1=堆/其他内存池
    } bulk;

    
    // 数组元素存储,64以内使用栈，64以上转移数据使用堆
    // 阶段1：内联小数组(64个元素)
    // 这里先考虑栈实现
    struct {
        // 存储解析的字符串
        // 这里嵌套数组也是一样
        // void* inline_elements[255];// 存储数据的指针,这里直接在stage中申请
        block_alloc_t inline_elements[255];// 存储数据的指针,这里直接在stage中申请
        // uint8_t inline_count;     // 内联数组中的元素数
        // 对于数组，这里存储的类型是block_alloc_t,这里先深拷贝
        // 阶段2：动态大数组
        struct {
            void** elements;      // 动态数组
            size_t count;         // 动态数组中的元素数
            size_t capacity;      // 动态数组容量
        } dynamic;
        
        read_state_t current_state;
        frame_type_t current_type;
        // 这里不需要帧类型
        // 因为都是简单类型和批量字符串
        // 直接用当前的元素的read_state_t状态就可以全部处理
        // 当前总元素数
        size_t total_count;
        //? 需要读到的元素个数
        size_t expected_count;
        
        // 当前模式
        enum {
            MODE_INLINE_ONLY,     // 仅使用内联数组（≤64）
            MODE_HYBRID,          // 使用动态（>64）
        } mode;
    } array;
    
    
    // 错误状态
    uint8_t has_error;
    char error_msg[128];
} resp_state_stack_t;
#endif