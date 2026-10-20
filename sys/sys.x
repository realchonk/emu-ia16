TARGET(binary)
INPUT(./user.bin)
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
		__user_begin = .;
		./user.bin
		__user_end = .;
	}
	.data : {
		*(.data)
	}
	__bss_begin = .;
	.bss : {
		*(COMMON)
		*(.bss)
	}
	__bss_end = .;
	__bss_size = __bss_end - __bss_begin;
}
