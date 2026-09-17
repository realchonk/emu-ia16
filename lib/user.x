ENTRY(_start)

SECTIONS {
	.text : {
		*(.text*)
	}

	.rodata : {
		*(.rodata*)
	}

	.data : {
		*(.data*)
	}

	.bss : {
		*(COMMON)
		*(.bss*)
	}

	__brk = .;

	. = 0x10000 - STACK_SIZE;
	__brk_end = .;

	ASSERT (__brk <= __brk_end, "Not enough size for stack!")
}
