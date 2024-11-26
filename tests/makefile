# Make file to build agent executable

CC					:= gcc
TARGET_EXEC			:= test
BUILD_DIR			:= ../build
TEST_OBJ_DIR		:= $(BUILD_DIR)/obj/test
CFLAGS				:= -lpthread -Wall -lz

IMPLANT_SOURCES=$(wildcard ../src/*.c)
IMPLANT_OBJECTS=$(patsubst %.c, $(TEST_OBJ_DIR)/%.o, $(notdir $(IMPLANT_SOURCES)))

AGENT_SOURCES=$(wildcard ../agent/src/*.c)
AGENT_OBJECTS=$(patsubst %.c, $(TEST_OBJ_DIR)/%.o, $(notdir $(AGENT_SOURCES)))

LISTENER_SOURCES=$(wildcard ../listener/src/*.c)
LISTENER_OBJECTS=$(patsubst %.c, $(TEST_OBJ_DIR)/%.o, $(notdir $(LISTENER_SOURCES)))

TEST_SOURCES		:= $(shell find . -name '*.c')
TEST_OBJECTS		:= $(patsubst %.c, $(TEST_OBJ_DIR)/%.o, $(TEST_SOURCES))

CFLAGS	+= -lcmocka -DUNIT_TESTING -ggdb
LINK	:=-L ../cmocka-1.1.7/build/src

# Includes
INC_FLAGS	:= -I../include -I../include/agent -I../include/listener 
TEST_INC	:= $(INC_FLAGS) -Iinclude/agent -Iinclude/listener -I../cmocka-1.1.7/include

# Build test executable
$(BUILD_DIR)/$(TARGET_EXEC): $(IMPLANT_OBJECTS) $(AGENT_OBJECTS) $(LISTENER_OBJECTS) $(TEST_OBJECTS)
	$(CC) $(LINK) -o $@ $(IMPLANT_OBJECTS) $(AGENT_OBJECTS) $(LISTENER_OBJECTS) $(TEST_OBJECTS) $(CFLAGS)

# Build implant source
$(IMPLANT_OBJECTS): $(TEST_OBJ_DIR)/%.o: ../src/%.c
	mkdir -p $(TEST_OBJ_DIR)
	$(CC) $(CFLAGS) $(TEST_INC) -c -o $@ $<

# Build agent source
$(AGENT_OBJECTS): $(TEST_OBJ_DIR)/%.o: ../agent/src/%.c
	mkdir -p $(TEST_OBJ_DIR)
	$(CC) $(CFLAGS) $(INC_FLAGS) -c -o $@ $<

# Build listener source
$(LISTENER_OBJECTS): $(TEST_OBJ_DIR)/%.o: ../listener/src/%.c
	mkdir -p $(TEST_OBJ_DIR)
	$(CC) $(CFLAGS) $(INC_FLAGS) -c -o $@ $<

# Build test source
$(TEST_OBJECTS): $(TEST_OBJ_DIR)/%.o: %.c
	mkdir -p $(@D)
	$(CC) $(LINK) $(CFLAGS) $(TEST_INC) -c -o $@ $<

.PHONY: test
test: $(BUILD_DIR)/test
	$(BUILD_DIR)/test

.PHONY: clean
clean:
	rm -r $(BUILD_DIR)