all: bin/listener bin/implant

bin/listener: listener/listener.c 
	gcc -o bin/listener listener/listener.c

bin/implant: implant/implant.c
	gcc -o bin/implant implant/implant.c

clean:
	rm -f $(ODIR)/*.o bin/listener bin/implant