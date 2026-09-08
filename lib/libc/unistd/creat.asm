[cpu 286]
[bits 16]

section .text
global creat
extern _sys
creat:
	mov ax, 8
	call far word [_sys]
	ret
