# test编译
# test : test.c ../arena_allocator.c
# 	gcc $^ -o $@


cc = gcc
version = c11
test_allocator_2 : test_allocator_2.c ../stage_allocator.c ../arena_allocator.c
	$(cc) -g -O0 -std=$(version) $^ -o $@ 