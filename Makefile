CXX 	:= g++
MKDIR	:= mkdir -p
RM		:= rm

LDFLAGS := -lfmt
FLAGS	:= -std=c++17 -O2 -Wall -Wextra

BUILD   := build
TARGET	:= checkers

SRCS 	:= main.cpp
OBJS 	:= $(SRCS:%=$(BUILD)/%.o)

$(BUILD)/$(TARGET): $(OBJS)
	$(CXX) $(OBJS) -o $@ $(LDFLAGS)

$(BUILD)/%.cpp.o: %.cpp
	$(MKDIR) $(dir $@)
	$(CXX) $(FLAGS) $(CXXFLAGS) -c $< -o $@

.PHONY: clean

clean:
	$(RM) -r $(BUILD)