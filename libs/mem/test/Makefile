# mem unit-тесты
#
#   make -C libs/mem/test
#   make -C libs/mem/test mem-check
#   make -C libs/mem/test pool-check

CXX      ?= c++
CXXFLAGS := -std=c++17 -Wall -Wextra -O0 -g -I../..

BUILD := build

.PHONY: all mem-check pool-check clean

all: mem-check pool-check

mem-check: $(BUILD)/mem_check
	$(BUILD)/mem_check

pool-check: $(BUILD)/range_pool_check
	$(BUILD)/range_pool_check

$(BUILD):
	mkdir -p $(BUILD)

$(BUILD)/mem_check: mem_check.cpp | $(BUILD)
	$(CXX) $(CXXFLAGS) -o $@ $<

$(BUILD)/range_pool_check: range_pool_check.cpp | $(BUILD)
	$(CXX) $(CXXFLAGS) -o $@ $<

clean:
	rm -rf $(BUILD)
