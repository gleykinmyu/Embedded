# MISC::TimerService / EvMsTimer / EvRetryTimer
#
#   make -C libs/Interfaces/test
#   make -C libs/Interfaces/test timer-check

CXX      ?= c++
CXXFLAGS := -std=c++17 -Wall -Wextra -O0 -g -I../.. -I..

BUILD := build

.PHONY: all timer-check clean

all: timer-check

timer-check: $(BUILD)/timer_check
	$(BUILD)/timer_check

$(BUILD):
	mkdir -p $(BUILD)

$(BUILD)/timer_check: timer_check.cpp ../ev_timer.hpp ../ms_timer.hpp ../../mem/list.hpp ../../mem/visit.hpp | $(BUILD)
	$(CXX) $(CXXFLAGS) -o $@ timer_check.cpp

clean:
	rm -rf $(BUILD)
