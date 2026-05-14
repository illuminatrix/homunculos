#include <stdint.h>
#include <string.h>
#include "gdt.h"

#define GDT_ENTRIES 6

struct gdt_entry {
	uint16_t limit_low;
	uint16_t base_low;
	uint8_t base_mid;
	uint8_t access;
	uint8_t granularity;
	uint8_t base_high;
} __attribute__((packed));

struct gdt_register {
	uint16_t limit;
	uint32_t base;
} __attribute__((packed));

struct tss_entry {
	uint32_t prev_tss;
	uint32_t esp0;
	uint32_t ss0;
	uint32_t esp1;
	uint32_t ss1;
	uint32_t esp2;
	uint32_t ss2;
	uint32_t cr3;
	uint32_t eip;
	uint32_t eflags;
	uint32_t eax;
	uint32_t ecx;
	uint32_t edx;
	uint32_t ebx;
	uint32_t esp;
	uint32_t ebp;
	uint32_t esi;
	uint32_t edi;
	uint32_t es;
	uint32_t cs;
	uint32_t ss;
	uint32_t ds;
	uint32_t fs;
	uint32_t gs;
	uint32_t ldt;
	uint16_t trap;
	uint16_t iomap_base;
} __attribute__((packed));

static struct gdt_entry gdt[GDT_ENTRIES];
static struct gdt_register gdtr;
static struct tss_entry tss;

static void gdt_set_entry(int idx, uint32_t base, uint32_t limit,
			  uint8_t access, uint8_t granularity)
{
	gdt[idx].base_low = base & 0xFFFF;
	gdt[idx].base_mid = (base >> 16) & 0xFF;
	gdt[idx].base_high = (base >> 24) & 0xFF;
	gdt[idx].limit_low = limit & 0xFFFF;
	gdt[idx].granularity = ((limit >> 16) & 0x0F) | (granularity & 0xF0);
	gdt[idx].access = access;
}

static void load_gdt(void)
{
	__asm__ volatile("lgdtl %0\n\t"
			 "ljmp %1, $1f\n\t"
			 "1:\n\t"
			 "mov %2, %%ds\n\t"
			 "mov %2, %%es\n\t"
			 "mov %2, %%fs\n\t"
			 "mov %2, %%gs\n\t"
			 "mov %2, %%ss\n\t"
			 :: "m"(gdtr),
			    "i"(GDT_KERNEL_CODE),
			    "r"(GDT_KERNEL_DATA)
			 : "memory");
}

static void load_tss(void)
{
	__asm__ volatile("ltr %%ax" :: "a"(GDT_TSS));
}

void gdt_init(void)
{
	memset(gdt, 0, sizeof(gdt));

	gdt_set_entry(1, 0, 0xFFFFF,
		      0x9A,  /* present, ring 0, code, execute/read */
		      0xCF); /* 4KB granularity, 32-bit, limit high */
	gdt_set_entry(2, 0, 0xFFFFF,
		      0x92,  /* present, ring 0, data, read/write */
		      0xCF);
	gdt_set_entry(3, 0, 0xFFFFF,
		      0xFA,  /* present, ring 3, code, execute/read */
		      0xCF);
	gdt_set_entry(4, 0, 0xFFFFF,
		      0xF2,  /* present, ring 3, data, read/write */
		      0xCF);

	memset(&tss, 0, sizeof(tss));
	tss.ss0 = GDT_KERNEL_DATA;
	tss.iomap_base = sizeof(tss);

	uint32_t tss_addr = (uint32_t)&tss;
	uint32_t tss_limit = sizeof(struct tss_entry) - 1;

	gdt_set_entry(5, tss_addr, tss_limit,
		      0x89,  /* present, ring 0, 32-bit TSS (available) */
		      0x40); /* 1-byte granularity, 32-bit TSS */

	gdtr.limit = sizeof(gdt) - 1;
	gdtr.base = (uint32_t)&gdt;

	load_gdt();
	load_tss();
}

void tss_set_kernel_stack(uint32_t esp0)
{
	tss.esp0 = esp0;
}
