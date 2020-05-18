TARGET_EXEC ?= a.out

BUILD_DIR ?= ./build
SRC_DIRS ?= ./staydb

SRCS := $(shell find $(SRC_DIRS) -name *.cpp -or -name *.c -or -name *.s)
OBJS := $(SRCS:%=$(BUILD_DIR)/%.o)
DEPS := $(OBJS:.o=.d)

# INC_DIRS := $(shell find $(SRC_DIRS) -type d)
INC_DIRS := .
INC_FLAGS := $(addprefix -I,$(INC_DIRS))

CPPFLAGS ?= $(INC_FLAGS) -MMD -MP 

$(BUILD_DIR)/$(TARGET_EXEC): $(OBJS)
	$(CXX) $(OBJS) -o $@ $(LDFLAGS) -Wall -pthread -llog4cplus -g -std=c++11

# assembly
$(BUILD_DIR)/%.s.o: %.s
	$(MKDIR_P) $(dir $@)
	$(AS) $(ASFLAGS) -c $< -o $@

# c source
$(BUILD_DIR)/%.c.o: %.c
	$(MKDIR_P) $(dir $@)
	$(CC) $(CPPFLAGS) $(CFLAGS) -c $< -o $@

# c++ source
$(BUILD_DIR)/%.cpp.o: %.cpp
	$(MKDIR_P) $(dir $@)
	$(CXX) $(CPPFLAGS) $(CXXFLAGS) -c $< -Wall -pthread -llog4cplus -g -std=c++11 -o $@


.PHONY: clean

clean:
	$(RM) -r $(BUILD_DIR)

run:
	-rm -r db
	-rm logging.txt
	./build/a.out

copy:
	cp inputs/*thread* judge
	cp inputs/data_prepare.txt judge

-include $(DEPS)

MKDIR_P ?= mkdir -p