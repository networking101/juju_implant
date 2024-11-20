CC			:= gcc
BUILD_DIR	:= ../build
OBJECT_DIR	:= $(BUILD_DIR)/obj
CFLAGS		:= -lpthread -Wall -lz

SOURCES=$(wildcard *.c)
OBJECTS=$(patsubst %.c, $(OBJECT_DIR)/%.o, $(SOURCES))

# Includes
INC_FLAGS	:= -I../include

debug: CFLAGS += -DDEBUG -ggdb

.PHONY: build
build: $(OBJECTS)

$(OBJECTS): $(OBJECT_DIR)/%.o: %.c
	mkdir -p $(OBJECT_DIR)
	$(CC) $(CFLAGS) $(INC_FLAGS) -c -o $@ $<