[bits 16]
section .text
extern write
global main

main:
	push bp
	mov bp, sp
	mov ax, 3
	push ax
	mov ax, msg
	push ax
	mov ax, 1
	push ax
	call write
	add sp, 6
	xor ax, ax
	pop bp
	ret

section .rodata
msg:
	db 111
	db 107
	db 10
