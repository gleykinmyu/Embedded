# Независимые тесты дополнений Bitsery. SMCP и comm не нужны.
#
#   make -C libs/bitsery/test check

CXX      ?= clang++
BITS     := $(abspath ..)
LIBS     := $(abspath $(BITS)/..)
BUILD    := build

CXXFLAGS := -std=c++17 -Wall -Wextra -Werror -fno-exceptions -fno-rtti \
	-I$(LIBS) \
	-I$(BITS)/include

.PHONY: check clean

check:
	@mkdir -p $(BUILD)
	$(CXX) $(CXXFLAGS) -O0 -g -o $(BUILD)/msb_bitpack_check msb_bitpack_check.cpp
	$(BUILD)/msb_bitpack_check
	$(CXX) $(CXXFLAGS) -O2 -DNDEBUG -o $(BUILD)/msb_bitpack_check_rel msb_bitpack_check.cpp
	$(BUILD)/msb_bitpack_check_rel
	$(CXX) $(CXXFLAGS) -O0 -g -fsanitize=address,undefined -fno-omit-frame-pointer -o $(BUILD)/msb_bitpack_check_san msb_bitpack_check.cpp
	$(BUILD)/msb_bitpack_check_san

clean:
	rm -rf $(BUILD)
