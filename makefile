BUILD_DIR	:= build

all:
	$(MAKE) -C agent
	$(MAKE) -C listener
	$(MAKE) -C tests

debug:
	$(MAKE) -C agent debug
	$(MAKE) -C listener debug
	

.PHONY: test
test: $(BUILD_DIR)/test
	./$(BUILD_DIR)/test

.PHONY: clean
clean:
	rm -r $(BUILD_DIR)



