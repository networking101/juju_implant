all: bin/listener bin/implant

bin/listener: listener/listener.c
	gcc -o bin/listener listener/listener.c listener/agent_handler.c -lpthread


bin/implant: implant/implant.c
	gcc -o bin/implant implant/implant.c

clean:
	rm -f $(ODIR)/*.o bin/listener bin/implant