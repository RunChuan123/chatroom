CXX := g++
CXXFLAGS := -std=c++17 -Wall -Wextra  -pedantic -g -O0
CPPFLAGS := -DDEBUG

.PHONY: all main

all: 
	./main

main: main.cpp
	$(CXX) $^ -o $@
# TARGET := main
# SRC := main.cpp window_op.cpp text_editor.cpp process_key.cpp file_op.cpp status.cpp
 
# .PHONY: all run clean
     
# all: $(TARGET)

# $(TARGET): $(SRC)
# 	$(CXX) $(CPPFLAGS) $(CXXFLAGS) $^ -o $@ 
# run: $(TARGET)
# 	./$(TARGET)
# clean:
# 	rm -f $(TARGET)

# row: row_key_show
# 	clang++ row_key_show.cpp -o row && ./row
