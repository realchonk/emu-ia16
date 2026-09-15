[cpu 286]
[bits 16]

section .text
global wait
extern _sys, _set_errno
$wait:
	mov ax, 11
	call far word [_sys]
	jmp _set_errno
