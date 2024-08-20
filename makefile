all:
	$(MAKE) -C src/agent
	$(MAKE) -C src/listener
	$(MAKE) -C tests

.PHONY: test
test: bin/test
	./bin/test

.PHONY: clean
clean:
	rm -f $(ODIR)/*.o bin/listener bin/agent bin/test



