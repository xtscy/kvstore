# test_interface:test_interface.c bpt_c.cc bpt.cc
# 	g++ -std=c++11 $^ -o $@
	
# test_cc:test.cc bpt_c.cc bpt.cc
# 	g++ -std=c++20 $^ -o $@
test_cc:test.cc  bpt.cc
	g++ -std=c++20 $^ -o $@ -g

.PHONY: clean
clean:
	rm -rf test_cc
