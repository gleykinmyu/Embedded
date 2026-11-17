# Host SMCP: bus + node (one Terminal = one Node)
#
#   make -C host
#   ./host/run_mesh.sh            # bus + 2 empty nodes (assign id in each)
#   open host/SMCP\ Bus.command   # macOS double-click

ROOT      := $(abspath ..)
EMBEDDED  := $(abspath $(ROOT)/../..)
SMCP_SRC  := $(EMBEDDED)/libs/smcp/smcp
SEG_SRC   := $(EMBEDDED)/libs/smcp/segment
IFACES    := $(EMBEDDED)/libs/Interfaces

CXX       ?= clang++
CXXFLAGS  := -std=c++17 -Wall -Wextra -O0 -g \
	-DSMCP_DEBUG \
	-DSMCP_TRACE_LINK \
	-I$(IFACES) \
	-I$(EMBEDDED)/libs \
	-I$(EMBEDDED)/libs/smcp \
	-I$(EMBEDDED)/libs/showFile \
	-I$(ROOT)/src \
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
SEGMENT   := $(BUILD)/segment_check

PUMS_SMCP := $(ROOT)/src/smcp
MEM_TEST  := $(EMBEDDED)/libs/mem/test
COMM_TEST := $(EMBEDDED)/libs/comm/test
SMCP_TEST := $(EMBEDDED)/libs/smcp/test

.PHONY: all clean dirs bus node check pool-check mem-check job-check proto-check \
	comm-check reply-hb-check proto-rest-check segment-check

all: dirs bus node $(CHECK) $(SEGMENT)

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

pool-check mem-check:
	$(MAKE) -C $(MEM_TEST) $@

job-check proto-check comm-check:
	$(MAKE) -C $(COMM_TEST) $@

REPLY_HB := $(BUILD)/reply_hb_check
NEW_LINK_HB := $(ROOT)/src/smcp/new_smcp/proto/link_hb.cpp

$(REPLY_HB): reply_hb_check.cpp $(NEW_LINK_HB) | dirs
	$(CXX) $(CXXFLAGS) -o $@ reply_hb_check.cpp $(NEW_LINK_HB)

reply-hb-check: $(REPLY_HB)
	$(REPLY_HB)

PROTO_REST := $(BUILD)/proto_rest_check

$(PROTO_REST): proto_rest_check.cpp $(NEW_LINK_HB) | dirs
	$(CXX) $(CXXFLAGS) -o $@ proto_rest_check.cpp $(NEW_LINK_HB)

proto-rest-check: $(PROTO_REST)
	$(PROTO_REST)

SEGMENT_OBJS := \
	$(BUILD)/console.o \
	$(BUILD)/show_console.o \
	$(BUILD)/server.o \
	$(BUILD)/axis.o

$(SEGMENT): $(SMCP_OBJS) $(SEGMENT_OBJS) $(BUILD)/segment_check.o
	$(CXX) $(CXXFLAGS) -o $@ $^

segment-check: $(SEGMENT)
	$(SEGMENT)

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
$(BUILD)/check_main.o: $(SMCP_TEST)/check_main.cpp sim_node.hpp hub_can.hpp error_log.hpp nack_fmt.hpp
	$(CXX) $(CXXFLAGS) -c -o $@ $<

$(BUILD)/console.o: $(SEG_SRC)/console.cpp $(SEG_SRC)/console.hpp | dirs
	$(CXX) $(CXXFLAGS) -c -o $@ $<
$(BUILD)/show_console.o: $(PUMS_SMCP)/show/console.cpp \
	$(PUMS_SMCP)/show/console.hpp $(SEG_SRC)/console.hpp | dirs
	$(CXX) $(CXXFLAGS) -c -o $@ $<
$(BUILD)/server.o: $(SEG_SRC)/server.cpp $(SEG_SRC)/server.hpp $(SEG_SRC)/message.hpp | dirs
	$(CXX) $(CXXFLAGS) -c -o $@ $<
$(BUILD)/axis.o: $(SEG_SRC)/axis.cpp $(SEG_SRC)/axis.hpp | dirs
	$(CXX) $(CXXFLAGS) -c -o $@ $<
$(BUILD)/segment_check.o: $(SMCP_TEST)/segment_check.cpp hub_can.hpp \
	$(PUMS_SMCP)/show/console.hpp $(SEG_SRC)/server.hpp | dirs
	$(CXX) $(CXXFLAGS) -c -o $@ $<

clean:
	rm -rf $(BUILD)
