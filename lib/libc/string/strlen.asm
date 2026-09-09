; size_t strlen(const char *s)
[bits 16]
[cpu 286]

section .text
global strlen
strlen:
	push bp
	mov bp, sp
	push di
	mov di, [bp+4]		; s
	xor ax, ax		; search for NUL
	mov cx, -1
	cld
	repne scasb
	lea ax, [di-1]		; address of NUL
	sub ax, [bp+4]		; minus start = length
	pop di
	pop bp
	ret
