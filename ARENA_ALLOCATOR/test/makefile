# test编译
# test : test.c ../arena_allocator.c
# 	gcc $^ -o $@


cc = gcc
version = c11
test_allocator : test_allocator.c ../stage_allocator.c ../arena_allocator.c
	$(cc) -std=$(version) $^ -o $@ 