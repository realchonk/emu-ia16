[cpu 286]
[bits 16]

section .text
global read
extern _sys
read:
	mov ax, 3
	call far word [_sys]
	ret
