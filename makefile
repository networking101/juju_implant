all:
	$(MAKE) -C agent
	$(MAKE) -C listener
	$(MAKE) -C tests

debug:
	$(MAKE) -C src/agent debug
	$(MAKE) -C src/listener debug
	

.PHONY: test
test: bin/test
	./bin/test

.PHONY: clean
clean:
	rm -f $(ODIR)/*.o bin/listener bin/agent bin/test



