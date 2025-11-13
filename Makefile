CXX = g++
CXXFLAGS = -std=c++11 -Wall -pthread -g
LDFLAGS = -pthread

COMMON_OBJS = common.o channel.o signals.o
SERVER_BINS = finance logging file
CLIENT_BIN = client

all: $(SERVER_BINS) $(CLIENT_BIN)

%.o: %.cpp
	$(CXX) $(CXXFLAGS) -c $< -o $@

finance: finance.o $(COMMON_OBJS) thread_pool.o
	$(CXX) $^ $(LDFLAGS) -o $@

logging: logging.o $(COMMON_OBJS)
	$(CXX) $^ $(LDFLAGS) -o $@

file: file.o $(COMMON_OBJS)
	$(CXX) $^ $(LDFLAGS) -o $@

client: client.o $(COMMON_OBJS)
	$(CXX) $^ $(LDFLAGS) -o $@

test:
	@make -s clean >/dev/null 
	@make -s all
	@$(CXX) $(CXXFLAGS) thread_pool_test.cpp $(COMMON_OBJS) thread_pool.cpp $(LDFLAGS) -o privatetest
	@bash lab4-tests.sh

clean:
	rm -f *.o $(SERVER_BINS) $(CLIENT_BIN)
	rm -f *.log
	rm -f fifo_*
	rm -rf storage
	rm -f test_*
	rm -rf test_results
	rm -f *_attributes.txt
	rm -f privatetest

.PHONY: all clean test