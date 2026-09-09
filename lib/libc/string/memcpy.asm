; void *memcpy(void *dest, const void *src, size_t num)
[bits 16]
[cpu 286]

section .text
global memcpy
memcpy:
	push bp
	mov bp, sp
	push si			; callee-saved
	push di			; callee-saved
	mov di, [bp+4]		; dest
	mov si, [bp+6]		; src
	mov cx, [bp+8]		; num
	mov ax, di		; return dest
	cld
	rep movsb
	pop di
	pop si
	pop bp
	ret
