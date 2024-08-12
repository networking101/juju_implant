INC=-Iinclude -Iinclude/agent -Iinclude/listener

all: bin/listener bin/implant

bin/listener: src/listener/listener.c
	gcc -g -o bin/listener $(INC) src/listener/listener.c src/queue.c src/listener/listener_comms.c src/listener/console.c src/listener/base.c src/listener/message_handler.c -lpthread


bin/implant: src/agent/agent.c
	gcc -g -o bin/agent $(INC) src/agent/agent.c src/queue.c src/agent/agent_comms.c src/agent/agent_message_handler.c -lpthread

clean:
	rm -f $(ODIR)/*.o bin/listener bin/agent