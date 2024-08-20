CC				:= gcc
INC				:=-I../include -I../include/agent -I../include/listener -I../cmocka-1.1.7/include/
TEST_INC		:=$(INC) -Iinclude/agent -Iinclude/listener

SRC				:=$(wildcard ../src/*.c) $(wildcard ../src/agent/*.c) $(wildcard ../src/listener/*.c)
TEST_SRC		:=$(SRC) $(wildcard *.c) $(wildcard agent_tests/*.c) $(wildcard listener_tests/*.c)

FLAGS			:=-lcmocka -DUNIT_TESTING -ggdb
LINK			:=-L ../cmocka-1.1.7/build/src

all: ../bin/test

debug: ../bin/test

../bin/test: test.c
	$(CC) $(TEST_INC) $(LINK) -o ../bin/test $(TEST_SRC) $(FLAGS)
	
.PHONY: clean
clean:
	rm -f ../bin/test