INC=-Iinclude -Isrc/agent/include -Isrc/listener/include

all: bin/listener bin/implant

bin/listener: src/listener/listener.c
	gcc -o bin/listener $(INC) src/listener/listener.c src/listener/listener_comms.c src/listener/console.c src/listener/globals.c src/queue.c src/listener/message_handler.c -lpthread


bin/implant: src/agent/agent.c
	gcc -o bin/agent $(INC) src/agent/agent.c src/agent/agent_comms.c

clean:
	rm -f $(ODIR)/*.o bin/listener bin/agent