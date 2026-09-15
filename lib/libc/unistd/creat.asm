[cpu 286]
[bits 16]

section .text
global creat
extern _sys, _set_errno
creat:
	mov ax, 8
	call far word [_sys]
	jmp _set_errno
