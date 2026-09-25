CXX = g++
CXXFLAGS = -std=c++17 -Wall -Wextra

all: server test_client

server: src/main.cpp
	$(CXX) $(CXXFLAGS) src/main.cpp -o server

test_client: tests/test_client.cpp
	$(CXX) $(CXXFLAGS) tests/test_client.cpp -o test_client

clean:
	rm -f server test_client *.o

.PHONY: all clean
