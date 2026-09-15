[cpu 286]
[bits 16]

section .text
global read
extern _sys, _set_errno
read:
	mov ax, 3
	call far word [_sys]
	jmp _set_errno
