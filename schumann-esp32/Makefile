CXX ?= g++
CXXFLAGS ?= -std=c++17 -Wall -Wextra -Werror -O2 -Isrc
BUILD := build

.PHONY: test clean

test: $(BUILD)/test_signal
	./$(BUILD)/test_signal

$(BUILD)/test_signal: test/test_signal.cpp src/signal.cpp src/signal.h
	mkdir -p $(BUILD)
	$(CXX) $(CXXFLAGS) -o $@ test/test_signal.cpp src/signal.cpp -lm

clean:
	rm -rf $(BUILD)
