; size_t strnlen(const char *s, size_t maxlen)
bits 16

section .text
global strnlen
strnlen:
	push bp
	mov bp, sp
	push di
	mov di, [bp+4]		; s
	mov cx, [bp+6]		; maxlen
	xor ax, ax		; search for NUL (also the maxlen == 0 result)
	cld
	jcxz .done		; nothing to scan
	repne scasb
	jne .full		; no NUL within maxlen
	mov ax, [bp+6]		; maxlen minus the remaining count gives
	sub ax, cx		; len+1 ...
	dec ax			; ... minus the NUL itself
	jmp .done
.full:
	mov ax, [bp+6]		; whole maxlen scanned
.done:
	pop di
	pop bp
	ret
