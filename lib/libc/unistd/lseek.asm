[cpu 286]
[bits 16]

section .text
global lseek
extern _sys, _set_errnol
lseek:
	mov ax, 5
	call far word [_sys]
	jmp _set_errnol
