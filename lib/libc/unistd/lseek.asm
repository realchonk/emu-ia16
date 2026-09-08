[cpu 286]
[bits 16]

section .text
global lseek
extern _sys
lseek:
	mov ax, 5
	call far word [_sys]
	ret
