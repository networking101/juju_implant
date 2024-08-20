CC				:= gcc
INC				:=-I../../include -I../../include/agent
SRC				:=$(wildcard ../*.c) $(wildcard *.c)

FLAGS			:=-lpthread

all: ../../bin/agent

debug: FLAGS += -DDEBUG -ggdb
debug: ../../bin/agent

../../bin/agent: agent.c
	$(CC) --static -o ../../bin/agent $(INC) $(SRC) $(FLAGS)
	
.PHONY: clean
clean:
	rm -f ../../bin/agent