#ifndef ELF_H
#define ELF_H

#include <stdint.h>

#define ELF_MAGIC 0x464C457F

#define ELFCLASS32 1
#define ELFDATA2LSB 1
#define ET_EXEC 2
#define EM_386 3

#define PT_LOAD 1

#define PF_X 1
#define PF_W 2
#define PF_R 4

struct elf32_ehdr {
	uint8_t e_ident[16];
	uint16_t e_type;
	uint16_t e_machine;
	uint32_t e_version;
	uint32_t e_entry;
	uint32_t e_phoff;
	uint32_t e_shoff;
	uint32_t e_flags;
	uint16_t e_ehsize;
	uint16_t e_phentsize;
	uint16_t e_phnum;
	uint16_t e_shentsize;
	uint16_t e_shnum;
	uint16_t e_shstrndx;
} __attribute__((packed));

struct elf32_phdr {
	uint32_t p_type;
	uint32_t p_offset;
	uint32_t p_vaddr;
	uint32_t p_paddr;
	uint32_t p_filesz;
	uint32_t p_memsz;
	uint32_t p_flags;
	uint32_t p_align;
} __attribute__((packed));

int elf_validate(const struct elf32_ehdr *ehdr);
int elf_load(const struct elf32_ehdr *ehdr, uint32_t *new_pdir,
	     uint32_t *entry_out);
void elf_copy_segments(const struct elf32_ehdr *ehdr);
uint32_t elf_brk_start(const struct elf32_ehdr *ehdr);

#endif
