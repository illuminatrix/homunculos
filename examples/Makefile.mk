EXAMPLES_DIR := examples
HELLO_ELF := $(EXAMPLES_DIR)/hello.elf
HELLO_EMBED := $(EXAMPLES_DIR)/hello_embed.o

$(HELLO_ELF): $(EXAMPLES_DIR)/hello.S
	@echo "AS $^    ->    $(EXAMPLES_DIR)/hello.o"
	@$(AS) $(ASFLAGS) -o $(EXAMPLES_DIR)/hello.o $^
	@echo "LD $(EXAMPLES_DIR)/hello.o    ->    $@"
	@$(LD) -melf_i386 -Ttext 0x08048000 -e _start -s -o $@ $(EXAMPLES_DIR)/hello.o

$(HELLO_EMBED): $(HELLO_ELF)
	@echo "EMBED $^    ->    $@"
	@$(OBJCOPY) -I binary -O elf32-i386 -B i386 $^ $@

OBJS += $(HELLO_EMBED)
