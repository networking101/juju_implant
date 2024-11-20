# Make file to build listener elf

CC					:= gcc
TARGET_EXEC			:= listener
BUILD_DIR			:= ../build
IMPLANT_OBJ_DIR		:= $(BUILD_DIR)/obj
LISTENER_OBJ_DIR	:= $(BUILD_DIR)/obj/listener
CFLAGS				:= -lpthread -Wall -lz

IMPLANT_SOURCES=$(wildcard ../src/*.c)
IMPLANT_OBJECTS=$(patsubst %.c, $(IMPLANT_OBJ_DIR)/%.o, $(IMPLANT_SOURCES))

LISTENER_SOURCES=$(wildcard src/*.c)
LISTENER_OBJECTS=$(patsubst %.c, $(LISTENER_OBJ_DIR)/%.o, $(notdir $(LISTENER_SOURCES)))

# Includes
INC_FLAGS	:= -I../include -I../include/listener

debug: CFLAGS += -DDEBUG -ggdb

# Build listener executable
$(BUILD_DIR)/$(TARGET_EXEC): $(IMPLANT_OBJECTS)# $(LISTENER_OBJECTS)
	$(CC) -o $@ $(IMPLANT_OBJECTS) #$(LISTENER_OBJECTS)

# Build implant source
$(IMPLANT_OBJECTS): $(IMPLANT_OBJ_DIR)/%.o: %.c
	mkdir -p $(IMPLANT_OBJ_DIR)
	$(CC) $(CFLAGS) $(INC_FLAGS) -c -o $@ $<

# Build listener source
$(LISTENER_OBJECTS): $(LISTENER_OBJ_DIR)/%.o: src/%.c
	mkdir -p $(LISTENER_OBJ_DIR)
	$(CC) $(CFLAGS) $(INC_FLAGS) -c -o $@ $<

.PHONY: clean
clean:
	rm -r $(BUILD_DIR)