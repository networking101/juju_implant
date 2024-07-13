all: bin/listener bin/implant

bin/listener: listener/listener.c
	gcc -o bin/listener listener/listener.c listener/agent_transmit.c listener/console.c -lpthread


bin/implant: implant/implant.c
	gcc -o bin/implant implant/implant.c implant/implant_transmit.c

clean:
	rm -f $(ODIR)/*.o bin/listener bin/implant