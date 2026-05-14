TESTS_DIR := $(dir $(abspath $(lastword $(MAKEFILE_LIST))))
KERNEL_DIR := $(TESTS_DIR)/..
KERNEL_BIN := $(KERNEL_DIR)/kernel.bin

TEST_SCRIPTS := test-multiboot test-boot test-vga test-shell test-usermode
TEST_SH := $(addprefix $(TESTS_DIR)/,$(addsuffix .sh,$(TEST_SCRIPTS)))

.PHONY: all test $(TEST_SCRIPTS) clean

all: test

$(KERNEL_BIN):
	@echo "Building kernel..."
	@$(MAKE) -C $(KERNEL_DIR) 2>&1

test: $(KERNEL_BIN) $(TEST_SCRIPTS)

test-multiboot: $(KERNEL_BIN)
	@$(TESTS_DIR)/test-multiboot.sh

test-boot: $(KERNEL_BIN)
	@$(TESTS_DIR)/test-boot.sh

test-vga: $(KERNEL_BIN)
	@$(TESTS_DIR)/test-vga.sh

test-shell: $(KERNEL_BIN)
	@$(TESTS_DIR)/test-shell.sh

test-usermode: $(KERNEL_BIN)
	@$(TESTS_DIR)/test-usermode.sh

clean:
	rm -f $(TESTS_DIR)/*.sock
	rm -f $(KERNEL_DIR)/serial.log

.PHONY: clean
