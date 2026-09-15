[cpu 286]
[bits 16]

section .text
global open
extern _sys, _set_errno
open:
	mov ax, 7
	call far word [_sys]
	jmp _set_errno
