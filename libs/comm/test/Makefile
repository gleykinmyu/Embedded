# comm unit-тесты (Job, Proto, Node/Session)
#
#   make -C libs/comm/test
#   make -C libs/comm/test job-check
#   make -C libs/comm/test proto-check
#   make -C libs/comm/test comm-check

CXX      ?= c++
CXXFLAGS := -std=c++17 -Wall -Wextra -O0 -g -I../.. -I../../Interfaces

BUILD := build

.PHONY: all job-check proto-check comm-check clean

all: job-check proto-check comm-check

job-check: $(BUILD)/job_check
	$(BUILD)/job_check

proto-check: $(BUILD)/proto_check
	$(BUILD)/proto_check

comm-check: $(BUILD)/comm_check
	$(BUILD)/comm_check

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
