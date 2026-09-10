[cpu 286]
[bits 16]

section .text
global wait
extern _sys
$wait:
	mov ax, 11
	call far word [_sys]
	ret
