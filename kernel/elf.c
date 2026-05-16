#include <stdint.h>
#include <stddef.h>
#include <string.h>
#include "elf.h"
#include "mm.h"
#include "gdt.h"

int
elf_validate(const struct elf32_ehdr *ehdr)
{
	if (!ehdr)
		return -1;

	if (*(uint32_t *)ehdr->e_ident != ELF_MAGIC)
		return -1;

	if (ehdr->e_ident[4] != ELFCLASS32)
		return -1;

	if (ehdr->e_ident[5] != ELFDATA2LSB)
		return -1;

	if (ehdr->e_type != ET_EXEC)
		return -1;

	if (ehdr->e_machine != EM_386)
		return -1;

	return 0;
}

int
elf_load(const struct elf32_ehdr *ehdr, uint32_t *new_pdir,
	 uint32_t *entry_out)
{
	int i;

	if (elf_validate(ehdr) < 0)
		return -1;

	if (!new_pdir || !entry_out)
		return -1;

	struct elf32_phdr *phdr = (struct elf32_phdr *)
		((uint32_t)ehdr + ehdr->e_phoff);

	for (i = 0; i < ehdr->e_phnum; i++) {
		if (phdr[i].p_type != PT_LOAD)
			continue;

		uint32_t va_start = phdr[i].p_vaddr & ~0xFFF;
		uint32_t va_end = (phdr[i].p_vaddr + phdr[i].p_memsz
				   + 0xFFF) & ~0xFFF;

		for (uint32_t va = va_start; va < va_end; va += 0x1000) {
			if (!mm_alloc_at(new_pdir, va,
					 MM_PRESENT | MM_RW | MM_USER))
				return -1;
		}
	}

	*entry_out = ehdr->e_entry;
	return 0;
}

void
elf_copy_segments(const struct elf32_ehdr *ehdr)
{
	struct elf32_phdr *phdr = (struct elf32_phdr *)
		((uint32_t)ehdr + ehdr->e_phoff);

	for (int i = 0; i < ehdr->e_phnum; i++) {
		if (phdr[i].p_type != PT_LOAD)
			continue;

		uint32_t src = (uint32_t)ehdr + phdr[i].p_offset;
		memcpy((void *)phdr[i].p_vaddr, (void *)src,
		       phdr[i].p_filesz);
	}
}
