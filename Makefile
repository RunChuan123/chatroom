# 自动检测编译器：优先 g++，否则用 clang++
CXX := $(shell which g++ || which clang++)
CXXFLAGS := -std=c++17 -Wall -Wextra -pedantic -g -O0
CPPFLAGS := -DDEBUG

# 根据平台选择 MySQL 的 include 和 lib
UNAME_S := $(shell uname -s)
ifeq ($(UNAME_S),Darwin)   # macOS
    MYSQL_INC := -I/opt/homebrew/opt/mysql-client/include
    MYSQL_LIB := -L/opt/homebrew/opt/mysql-client/lib -lmysqlclient
else ifeq ($(UNAME_S),Linux)   # Linux
    MYSQL_INC := -I/usr/include/mysql
    MYSQL_LIB := -L/usr/lib/x86_64-linux-gnu -lmysqlclient
endif

SRCS := main.cpp db_op.cpp thread.cpp test.cpp
OBJS := $(SRCS:.cpp=.o)
TARGET := main

.PHONY: all clean run

all: $(TARGET)

$(TARGET): $(OBJS)
	$(CXX) $(CXXFLAGS) $(CPPFLAGS) $(MYSQL_INC) $(OBJS) -o $@ $(MYSQL_LIB)

%.o: %.cpp
	$(CXX) $(CXXFLAGS) $(CPPFLAGS) $(MYSQL_INC) -c $< -o $@

run: $(TARGET)
	./$(TARGET)

clean:
	rm -f $(OBJS) $(TARGET)
