CC				:= gcc
INC				:=-I../../include -I../../include/listener
SRC				:=$(wildcard ../*.c) $(wildcard *.c)
LIB		 		:=../../libs/zlib-1.3.1/libz.a

FLAGS			:=-lpthread -Wall -lz

all: ../../bin/listener

debug: FLAGS += -DDEBUG -ggdb
debug: ../../bin/listener

../../bin/listener: listener.c
	$(CC) -o ../../bin/listener $(INC) $(SRC) $(FLAGS) 
	
.PHONY: clean
clean:
	rm -f ../../bin/listener