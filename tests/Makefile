TESTS_DIR := $(dir $(abspath $(lastword $(MAKEFILE_LIST))))
KERNEL_DIR := $(TESTS_DIR)/..
KERNEL_BIN := $(KERNEL_DIR)/kernel.bin

TEST_SCRIPTS := test-multiboot test-boot test-vga test-shell test-usermode test-mmap test-exec test-tmpfs test-ext2
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

test-mmap: $(KERNEL_BIN)
	@$(TESTS_DIR)/test-mmap.sh

test-exec: $(KERNEL_BIN)
	@$(TESTS_DIR)/test-exec.sh

test-tmpfs: $(KERNEL_BIN)
	@$(TESTS_DIR)/test-tmpfs.sh

test-ext2: $(KERNEL_BIN)
	@$(TESTS_DIR)/test-ext2.sh

clean:
	rm -f $(TESTS_DIR)/*.sock
	rm -f $(KERNEL_DIR)/serial.log

.PHONY: clean
