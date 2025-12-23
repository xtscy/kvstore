# kvstore : hook_tcpserver.c kv_task.c kv_func.c kv_protocal.c kvstore.c kv_reactor.c ./RingBuffer/ring_buffer.c ./threadpool/threadpool.c ./lock_free_ring_buf/lock_free_ring_buf.c
# 	gcc $^ -o $@ -I ./NtyCo-master/core -L./NtyCo-master/ -lntyco
# kvstore :       
# 	g++ $^ -o $@  -std=c++14 -D_GNU_SOURCE 

CC = gcc
CXX = g++
CFLAGS = -std=c11 -D_GNU_SOURCE
CXXFLAGS = -std=c++17

C_SRCS = hook_tcpserver.c kv_task.c kv_func.c kv_protocal.c kvstore.c kv_reactor.c ./RingBuffer/ring_buffer.c ./threadpool/threadpool.c \
		./lock_free_ring_buf/lock_free_ring_block_buf.c ./ARENA_ALLOCATOR/stage_allocator.c \
		./ARENA_ALLOCATOR/arena_allocator.c \
		./memory_pool/memory_pool.c \
		./Sequence_lock/Sequence_lock.c \
		./MemoryBTreeLock/m_btree.c
CPP_SRCS = ./BPlusTree/bpt_c.cc ./BPlusTree/bpt.cc ./Persister/persister_c.cc ./Persister/persister.cc
C_OBJS = $(C_SRCS:.c=.o)
CPP_OBJS = $(CPP_SRCS:.cc=.o)
TARGET = kvstore

.PHONY:clean
all:$(TARGET)
%.o:%.c
	$(CC) $(CFLAGS) -c $< -o $@ -g 

%.o:%.cc
	$(CXX) $(CXXFLAGS) -c $< -o $@ -g 

$(TARGET):$(C_OBJS) $(CPP_OBJS)
	$(CXX) $^ -o $@ -g -I ./NtyCo-master/core  -L./NtyCo-master/ -lntyco 2>&1 | tee build.log

clean_all : clean clean_log clean_full_log

clean:
	rm -f $(C_OBJS) $(CPP_OBJS) $(TARGET) 
clean_log:
	rm -f incremental_*.log
clean_full_log:
	rm -f fullLog.db