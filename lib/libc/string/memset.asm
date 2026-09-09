; void *memset(void *dest, int val, size_t num)
[bits 16]
[cpu 286]

section .text
global memset
memset:
	push bp
	mov bp, sp
	push di
	mov di, [bp+4]		; dest
	mov ax, [bp+6]		; val (only AL is used)
	mov cx, [bp+8]		; num
	mov dx, di		; remember dest for return
	cld
	rep stosb
	mov ax, dx
	pop di
	pop bp
	ret
