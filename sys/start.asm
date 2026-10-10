[bits 16]
[cpu 286]

section .boot
global _start
_start:
	mov ax, 0xb800
	mov es, ax
	mov byte [es:0], 'X'

_halt:
	 cli
	 hlt
	 jmp _start
