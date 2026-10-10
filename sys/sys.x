ENTRY(_start)
OUTPUT_FORMAT(elf32-i386)

SECTIONS {
	IVT = 0;
	BDA = 0x400;

	. = 0x600;
	
	.text : {
		*(.boot)
		*(.text)
	}
	.rodata : {
		*(.rodata*)
	}
	.data : {
		*(.data)
	}
	.bss : {
		*(COMMON)
		*(.bss)
	}
}
