# include "hash.h"


// 这里接收外部传入的hash_table

// hash_table_t global_hash_table = {0};
hash_table_t *global_hash_table = {0};


hash_table_t* hash_table_create(size_t size, hash_table_t *table) {

    //这里先直接malloc开辟，后续可以用一个内存池
    // 向内存池中申请，在一个内存池中管理多种数据结构
    // 这里init一般直接malloc都差不多，因为只会调用一次

    //这里可以判断一下size是否是质数    
    hash_table_t *new_table = malloc(sizeof(hash_table_t));
    
    new_table->table = malloc(sizeof(hash_item_t) * size);
    if (new_table->table == NULL) {
        printf("malloc hash item array failed\n");
        exit(-3);
    }

    memset(new_table, 0, sizeof(hash_item_t) * size);


    new_table->size = size;
    
    return new_table;
}

// 这里实现对外的字符串获取整形key

int abs_val(int val) {
    return val < 0 ? -val : val;
}

// 双重哈希
// 第一个哈希，直接取模得到位置
// 第二个哈希，计算遍历的步长

unsigned long hash_init_pos(unsigned long key, hash_table_t *table) {
    return key % table->size;
}

unsigned long hash_step_size(unsigned long key, hash_table_t *table) {
    // 获得相对于size互质的步长
    // 互质就能遍历到每一个位置,不会提前回到起点
    // 当然这里table的size一定是质数才行
    // 这里拿范围[1, size - 1], 因为size是质数，所以每个数都必然互质，那么每个数都可以是步长
    return 1 + (key % (table->size - 1));
}

unsigned long string_to_key(const char *input) {

    unsigned long hash = 5381;
    int c;
    
    while ((c = *input++)) {
        hash = ((hash << 5) + hash) + c; // hash * 33 + c
    }
    return hash;
}

void destroy_table(hash_table_t *table) {

    if (table) {
        free(table->table);
        free(table);
    }
    
}
// 这里拿到当前位置的key, 判断当前table位置的key
// 这里key是数组下标，并不是实际存储的void*的key数据
// 这里传数组下标，还需要把新的key传过来
// 这里判断key_pos位置是否key_data和当前void*的二进制位相同
// 在判断之前
bool key_compare(hash_item_t *item, void *key_data, uint16_t key_size) {

    // 这里首先长度必须相同，然后就是每个字节都相同
    // 那么就认定两个key是相同的
    if (item->key_size != key_size) return false;

    if(!memcmp(item->key, key_data, key_size)) {
        return true;
    }
    
    return false;
}


// 这里实现多套API针对不同的类型
// 由于key永远都是存储字符串
// 所以下面的insert可以直接复用，但是拿的时候就不知道存的是什么类型，所以
// 还是需要增添一个标识类型的枚举字段
// 这里用户外部调用函数，把非整形类型转换成整形类型
// 这里不允许有重复的键
// 就算类型不同，键也不能相同
bool insert(unsigned long key_pos, void *key_data, uint16_t key_size,
    void* val, uint16_t val_size, hash_table_t *table, value_type_t type) {

    if (key_data == NULL || val == NULL || table == NULL) {
        printf("insert parameter NULL\n");
        exit(-3);
    }
    // 先通过hash1获取初始位置
    int init_pos = hash_init_pos(key_pos, table);
    int step = hash_step_size(key_pos, table);
    int cur_pos = 0;
    hash_item_t *cur_item = {0};
    for (int i = 0; i < table->size; i++) {
        cur_pos = (init_pos + step * i) % table->size;
        //* 这里判断，key_size相同并且key数据相同那么才会认定为同一个
        //! 这里判断当前位置是否被删除或者是被
        cur_item = &table->table[cur_pos];
        if (!cur_item->is_occupied || cur_item->is_deleted) {
            if (cur_item->is_deleted) {
                free(cur_item->key);
                free(cur_item->val);
            }
            // 直接使用当前位置
            cur_item->is_occupied = true;
            cur_item->key = key_data;
            cur_item->key_size = key_size;
            cur_item->val = val;
            cur_item->val_size = val_size;
            cur_item->is_deleted = false;
            cur_item->val_type = type;
            return true;
        }
        
        if (key_compare(&table->table[cur_pos], key_data, key_size)) {
            // 如果判断成功，即下标位置的key相同，直接更改value值
            // 那么这里还需要回收之前的值吗
            //! 当前是直接malloc开辟的值,那么直接free即可
            //! 后续有数据内存池，就调用外部free接口
            free(cur_item->val);
            table->table[cur_pos].val = val;
            table->table[cur_pos].val_size = val_size;
            table->table[cur_pos].val_type = type;
            return true;
        }
        
        // 继续循环
    }
    
    // 没有能够插入数据的位置,这里要实现一个自动扩容机制，当负载因子达到某个阈值时，进行扩容
    // 这里先就这样，如果没有位置插入，那么直接返回false;
    return false;
}


void* hash_table_search(unsigned long key, void *key_data, uint16_t key_size, 
    value_type_t *type, hash_table_t *table) {
    unsigned long init_pos = 0, step_size = 0, cur_pos = 0;
    init_pos = hash_init_pos(key, table);
    step_size = hash_step_size(key, table);
    for (int i = 0; i < table->size; i++) {
        // 判断当前位置
        cur_pos = (init_pos + step_size * i) % table->size;
        // 没被占用或者被删除,说明当前位置是空
        if (!table->table[cur_pos].is_occupied || table->table[cur_pos].is_deleted) {
            return NULL;
        }
        // 当前位置为有效值，判断是否和搜索键相同

        if (key_compare(&table[cur_pos], key_data, key_size)) {
            *type = table->table[cur_pos].val_type;
            return table->table[cur_pos].val;
        }
    }
    // 遍历完整个数组，不存在该键
    return NULL;    
}

bool hash_table_delete(unsigned long key, void *key_data, uint16_t key_size, hash_table_t *table) {

    // 遍历table找到相同key
    unsigned long init_pos = 0, step_size = 0, cur_pos = 0;
    init_pos = hash_init_pos(key, table);
    cur_pos = hash_step_size(key, table);
    for (int i = 0; i < table->size; i++) {
        cur_pos = (init_pos + step_size * i) % table->size;
        if (!table->table[cur_pos].is_occupied || table->table[cur_pos].is_deleted) {
            return false;
        }
        if (key_compare(&table->table[cur_pos], key_data, key_size)) {
            table->table[cur_pos].is_deleted = true;
            return true;
        }
    }   
    return false;
}


void hash_table_print(hash_table_t *table) {
    hash_item_t *temp_item = NULL;
    value_type_t type = 0;
    char temp_buffer[257] = {0};// 这里key不超过256
    int int_array[200] = {0};
    double double_array[200] = {0};
    char* string_array[200] = {0};
    int int_cpos = 0, double_cpos = 0, string_cpos = 0;
    int string_size = 0;
    for (int i = 0; i < table->size; i++) {
        temp_item = &table->table[i];
        type = temp_item->val_type;

        if (type == Int) {

            int_array[int_cpos++] = *(long*)temp_item->val;
            
            // snprintf保证最后一个位置是字符串结束标志
            // snprintf(temp_buffer, temp_item->key_size, "%s", temp_item->key);
            // printf("pos:%d key: %s || value: %ld\n", i, temp_buffer, *((long*)temp_item->val));
            
        } else if (type == Double) {
            
            double_array[double_cpos++] = *(double*)temp_item->val;
            
            // snprintf(temp_buffer, temp_item->key_size, "%s", temp_item->key);
            // printf("pos:%d key: %s || value: %lf\n", i, temp_buffer, *((double*)temp_item->val));
            
            
        } else if (type == String) {

            string_size = temp_item->val_size;
            
            char *m_string = malloc(string_size + 1);
            memset(m_string, 0, string_size + 1);
            memcpy(m_string, temp_item->val, temp_item->val_size);


            
            string_array[string_cpos++] = m_string;
            
            // snprintf(temp_buffer, temp_item->key_size, "%s", temp_item->key);
            // printf("pos:%d key: %s || value: %s\n", i, temp_buffer, (char*)temp_item->val);
            
        } 
    }    

    for (int i = 0; i < int_cpos; i++) {

        printf("int %d : %d\n", i, int_array[i]);
    }

    for (int i = 0; i < double_cpos; i++) {

        printf("double %d : %lf\n", i, double_array[i]);
    }
    
    for (int i = 0; i < int_cpos; i++) {

        printf("string %d : %s\n", i, string_array[i]);
    }
}



