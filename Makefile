CXX	= g++
CXXFLAGS	= -Wall -Wextra -Werror -ansi -pedantic -O3
LIBS	= -lpthread -lrt

all: test_mutex test_mutex_check

test_mutex: test_mutex.cpp
	$(CXX) test_mutex.cpp -o test_mutex $(CXXFLAGS) $(LIBS) -DNOCHECKS=1

test_mutex_check: test_mutex.cpp
	$(CXX) test_mutex.cpp -o test_mutex_check $(CXXFLAGS) $(LIBS)

clean:
	rm -f test_mutex test_mutex_check
