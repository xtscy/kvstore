all : set_test get_test

get_test : get_test.cc
	g++ -std=c++20 -o $@ $^

set_test : test.cc
	g++ -std=c++20 -o $@ $^

clean:
	rm -rf get_test set_test 