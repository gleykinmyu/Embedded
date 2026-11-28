# comm unit-тесты (Job, Proto, Node/Session)
#
#   make -C libs/comm/test
#   make -C libs/comm/test job-check
#   make -C libs/comm/test proto-check
#   make -C libs/comm/test comm-check

CXX      ?= c++
CXXFLAGS := -std=c++17 -Wall -Wextra -O0 -g -I../.. -I../../Interfaces
STRICT   := -std=c++17 -Wall -Wextra -Werror -fno-exceptions -fno-rtti \
	-I../.. -I../../Interfaces -I../../bitsery/include -I../../../PUMS/Console/src

BUILD := build

.PHONY: all job-check proto-check comm-check wire-check stack-check clean

all: job-check wire-check stack-check proto-check comm-check

job-check: $(BUILD)/job_check
	$(BUILD)/job_check

proto-check: $(BUILD)/proto_check
	$(BUILD)/proto_check

comm-check: $(BUILD)/comm_check
	$(BUILD)/comm_check

wire-check: | $(BUILD)
	$(CXX) $(STRICT) -O0 -g -o $(BUILD)/frame_check frame_check.cpp
	$(BUILD)/frame_check
	$(CXX) $(STRICT) -O2 -DNDEBUG -o $(BUILD)/frame_check_rel frame_check.cpp
	$(BUILD)/frame_check_rel
	$(CXX) $(STRICT) -O0 -g -fsanitize=address,undefined -fno-omit-frame-pointer -o $(BUILD)/frame_check_san frame_check.cpp
	$(BUILD)/frame_check_san

stack-check: | $(BUILD)
	$(CXX) $(STRICT) -O0 -g -o $(BUILD)/stack_check stack_check.cpp
	$(BUILD)/stack_check
	$(CXX) $(STRICT) -O2 -DNDEBUG -o $(BUILD)/stack_check_rel stack_check.cpp
	$(BUILD)/stack_check_rel
	$(CXX) $(STRICT) -O0 -g -fsanitize=address,undefined -fno-omit-frame-pointer -o $(BUILD)/stack_check_san stack_check.cpp
	$(BUILD)/stack_check_san

$(BUILD):
	mkdir -p $(BUILD)

$(BUILD)/job_check: job_check.cpp | $(BUILD)
	$(CXX) $(CXXFLAGS) -o $@ $<

$(BUILD)/proto_check: proto_check.cpp | $(BUILD)
	$(CXX) $(CXXFLAGS) -o $@ $<

$(BUILD)/comm_check: comm_check.cpp | $(BUILD)
	$(CXX) $(CXXFLAGS) -o $@ $<

clean:
	rm -rf $(BUILD)
