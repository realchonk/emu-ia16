[cpu 286]
[bits 16]

section .text
global open
extern _sys
open:
	mov ax, 7
	call far word [_sys]
	ret
