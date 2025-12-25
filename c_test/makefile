all : set_test get_test sort_test

get_test : get_test.cc
	g++ -std=c++20 -o $@ $^

set_test : test.cc
	g++ -std=c++20 -o $@ $^


sort_test : sort_test.cc
	g++ -std=c++17 -o $@ $^

	
PHONY:clean
clean:
	rm -rf get_test set_test sort_test