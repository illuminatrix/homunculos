ENTRY(_start)
 
SECTIONS
{
	. = 1M;
 
	.text BLOCK(4K) : ALIGN(4K)
	{
		*(.multiboot)
		*(.text)
	}

	.rodata BLOCK(4K) : ALIGN(4K)
	{
		*(.rodata)
	}

	.data BLOCK(4K) : ALIGN(4K)
	{
		*(.data)
		. = ALIGN(4);
		__driver_init_start = .;
		KEEP(*(.driver_init))
		__driver_init_end = .;
	}
 
	.bss BLOCK(4K) : ALIGN(4K)
	{
		*(COMMON)
		*(.bss)
		. = ALIGN(4K);
		__kernel_end = .;
	}
}
