# Make file to build agent executable

CC					:= gcc
TARGET_EXEC			:= agent
BUILD_DIR			:= ../build
IMPLANT_OBJ_DIR		:= $(BUILD_DIR)/obj
AGENT_OBJ_DIR		:= $(BUILD_DIR)/obj/agent
CFLAGS				:= -lpthread -Wall -lz

IMPLANT_SOURCES=$(wildcard ../src/*.c)
IMPLANT_OBJECTS=$(patsubst %.c, $(IMPLANT_OBJ_DIR)/%.o, $(notdir $(IMPLANT_SOURCES)))

AGENT_SOURCES=$(wildcard src/*.c)
AGENT_OBJECTS=$(patsubst %.c, $(AGENT_OBJ_DIR)/%.o, $(notdir $(AGENT_SOURCES)))

# Includes
INC_FLAGS	:= -I../include -I../include/agent

debug: CFLAGS += -DDEBUG -ggdb
debug: $(BUILD_DIR)/$(TARGET_EXEC)

# Build agent executable
$(BUILD_DIR)/$(TARGET_EXEC): $(IMPLANT_OBJECTS) $(AGENT_OBJECTS)
	$(CC) -o $@ $(IMPLANT_OBJECTS) $(AGENT_OBJECTS)

# Build implant source
$(IMPLANT_OBJECTS): $(IMPLANT_OBJ_DIR)/%.o: ../src/%.c
	mkdir -p $(IMPLANT_OBJ_DIR)
	$(CC) $(CFLAGS) $(INC_FLAGS) -c -o $@ $<

# Build agent source
$(AGENT_OBJECTS): $(AGENT_OBJ_DIR)/%.o: src/%.c
	mkdir -p $(AGENT_OBJ_DIR)
	$(CC) $(CFLAGS) $(INC_FLAGS) -c -o $@ $<

.PHONY: clean
clean:
	rm -r $(BUILD_DIR)