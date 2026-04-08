# Compiler
CXX = g++
CXXFLAGS = -Wall -Wextra -std=c++17 -pthread

# Source files
SRCS = main.cpp dht.cpp ht.cpp network.cpp dist_barrier.cpp

# Object files (replace .cpp with .o)
OBJS = $(SRCS:.cpp=.o)

# Executable name
TARGET = my_program

# Default target
all: $(TARGET)

# Link object files to create the executable
$(TARGET): $(OBJS)
	$(CXX) $(CXXFLAGS) -o $@ $(OBJS)

# Compile .cpp files to .o files
%.o: %.cpp
	$(CXX) $(CXXFLAGS) -c $< -o $@

# Clean up object files and executable
clean:
	rm -f $(OBJS) $(TARGET)

.PHONY: all clean
