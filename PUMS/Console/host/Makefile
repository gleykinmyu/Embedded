# Host SMCP: bus + node (one Terminal = one Node)
#
#   make -C host
#   ./host/run_mesh.sh            # bus + 2 empty nodes (assign id in each)
#   open host/SMCP\ Bus.command   # macOS double-click

ROOT      := $(abspath ..)
EMBEDDED  := $(abspath $(ROOT)/../..)
SMCP_SRC  := $(EMBEDDED)/libs/smcp/smcp
IFACES    := $(EMBEDDED)/libs/Interfaces

CXX       ?= clang++
CXXFLAGS  := -std=c++17 -Wall -Wextra -O0 -g \
	-DSMCP_DEBUG \
	-DSMCP_TRACE_LINK \
	-I$(IFACES) \
	-I$(EMBEDDED)/libs/smcp \
	-I.

BUILD     := build
BUS       := $(BUILD)/smcp_bus
NODE      := $(BUILD)/smcp_node

SMCP_OBJS := \
	$(BUILD)/ilink.o \
	$(BUILD)/can_link.o \
	$(BUILD)/message.o \
	$(BUILD)/multi_byte.o \
	$(BUILD)/node.o \
	$(BUILD)/session.o \
	$(BUILD)/link_hb.o \
	$(BUILD)/reply_wnd.o \
	$(BUILD)/tagged_ops.o \
	$(BUILD)/debug.o

CHECK     := $(BUILD)/smcp_check

.PHONY: all clean dirs bus node check

all: dirs bus node $(CHECK)

bus: dirs $(BUS)
node: dirs $(NODE)

dirs:
	@mkdir -p $(BUILD)

# object files need build/ to exist (parallel make after clean)
$(SMCP_OBJS) $(BUILD)/socket_can.o $(BUILD)/debug_trace.o $(BUILD)/node_main.o $(BUILD)/bus_main.o $(BUILD)/check_main.o: | dirs

$(BUS): $(BUILD)/bus_main.o
	$(CXX) $(CXXFLAGS) -o $@ $^

$(NODE): $(SMCP_OBJS) $(BUILD)/socket_can.o $(BUILD)/debug_trace.o $(BUILD)/node_main.o
	$(CXX) $(CXXFLAGS) -o $@ $^ -lreadline

$(CHECK): $(SMCP_OBJS) $(BUILD)/check_main.o
	$(CXX) $(CXXFLAGS) -o $@ $^

check: $(CHECK)
	$(CHECK)

$(BUILD)/ilink.o: $(SMCP_SRC)/ilink.cpp $(SMCP_SRC)/ilink.hpp $(SMCP_SRC)/message.hpp
	$(CXX) $(CXXFLAGS) -c -o $@ $<
$(BUILD)/can_link.o: $(SMCP_SRC)/can_link.cpp $(SMCP_SRC)/can_link.hpp $(SMCP_SRC)/ilink.hpp
	$(CXX) $(CXXFLAGS) -c -o $@ $<
$(BUILD)/message.o: $(SMCP_SRC)/message.cpp $(SMCP_SRC)/message.hpp
	$(CXX) $(CXXFLAGS) -c -o $@ $<
$(BUILD)/multi_byte.o: $(SMCP_SRC)/session/multi_byte.cpp $(SMCP_SRC)/session/multi_byte.hpp
	$(CXX) $(CXXFLAGS) -c -o $@ $<
$(BUILD)/node.o: $(SMCP_SRC)/node.cpp $(SMCP_SRC)/node.hpp $(SMCP_SRC)/session.hpp
	$(CXX) $(CXXFLAGS) -c -o $@ $<
$(BUILD)/session.o: $(SMCP_SRC)/session.cpp $(SMCP_SRC)/session.hpp $(SMCP_SRC)/node.hpp \
	$(SMCP_SRC)/session/link_hb.hpp $(SMCP_SRC)/session/reply_wnd.hpp $(SMCP_SRC)/session/tagged_ops.hpp
	$(CXX) $(CXXFLAGS) -c -o $@ $<
$(BUILD)/link_hb.o: $(SMCP_SRC)/session/link_hb.cpp $(SMCP_SRC)/session/link_hb.hpp $(SMCP_SRC)/session.hpp
	$(CXX) $(CXXFLAGS) -c -o $@ $<
$(BUILD)/reply_wnd.o: $(SMCP_SRC)/session/reply_wnd.cpp $(SMCP_SRC)/session/reply_wnd.hpp $(SMCP_SRC)/session.hpp
	$(CXX) $(CXXFLAGS) -c -o $@ $<
$(BUILD)/tagged_ops.o: $(SMCP_SRC)/session/tagged_ops.cpp $(SMCP_SRC)/session/tagged_ops.hpp $(SMCP_SRC)/session.hpp
	$(CXX) $(CXXFLAGS) -c -o $@ $<
$(BUILD)/debug.o: $(SMCP_SRC)/debug.cpp $(SMCP_SRC)/debug.hpp
	$(CXX) $(CXXFLAGS) -c -o $@ $<

$(BUILD)/socket_can.o: socket_can.cpp socket_can.hpp
	$(CXX) $(CXXFLAGS) -c -o $@ $<
$(BUILD)/bus_main.o: bus_main.cpp wire_frame.hpp
	$(CXX) $(CXXFLAGS) -c -o $@ $<
$(BUILD)/debug_trace.o: debug_trace.cpp nack_fmt.hpp
	$(CXX) $(CXXFLAGS) -c -o $@ $<
$(BUILD)/node_main.o: node_main.cpp sim_node.hpp socket_can.hpp clock.hpp error_log.hpp nack_fmt.hpp
	$(CXX) $(CXXFLAGS) -c -o $@ $<
$(BUILD)/check_main.o: check_main.cpp sim_node.hpp hub_can.hpp error_log.hpp nack_fmt.hpp
	$(CXX) $(CXXFLAGS) -c -o $@ $<

clean:
	rm -rf $(BUILD)
