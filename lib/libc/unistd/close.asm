[cpu 286]
[bits 16]

section .text
global close
extern _sys
close:
	mov ax, 4
	call far word [_sys]
	ret
