; ldivmod.asm -- signed 32/16 division and remainder for c0's
; constant folding, in the division style of the old C libraries.
;
;	long ldivmod (long l, int r, int wantmod)
;
; divides l by r; r must be nonzero and fit in a signed word.  The
; quotient or, with wantmod, the remainder is returned in dx:ax, with
; C semantics: the remainder carries the sign of the dividend.
;
; The division is unsigned on absolute values, in two hardware
; steps: first the high word, then (remainder:low word); both are
; 32/16 divisions that cannot overflow, since the remainder of the
; first step is smaller than the divisor.

bits 16
section .text
global ldivmod

; stack: [bp+4] dividend low word, [bp+6] dividend high word,
;	 [bp+8] divisor, [bp+10] wantmod
; si flags: 1 = dividend was negative, 2 = divisor was negative
ldivmod:
	push	bp
	mov	bp, sp
	push	bx
	push	si
	push	di

	xor	si, si
	mov	bx, [bp+8]		; divisor
	test	bx, bx
	jns	.rpos
	neg	bx
	or	si, 2
.rpos:
	mov	ax, [bp+4]		; dividend
	mov	dx, [bp+6]
	test	dx, dx
	jns	.lpos
	not	ax
	not	dx
	add	ax, 1
	adc	dx, 0
	or	si, 1
.lpos:
	mov	di, ax			; low word

	mov	ax, dx			; high word / divisor
	xor	dx, dx
	div	bx			; ax = quotient high, dx = remainder
	mov	cx, ax
	mov	ax, di			; (remainder:low) / divisor
	div	bx			; ax = quotient low, dx = remainder

	cmp	word [bp+10], 0
	je	.quot

	mov	ax, dx			; remainder: dx:ax = 0:rem
	xor	dx, dx
	test	si, 1			; sign of the dividend
	jnz	.negres
	jmp	.done

.quot:
	mov	dx, cx			; dx:ax = quotient
	mov	di, si
	shr	di, 1
	xor	di, si			; bit 0: signs differ
	test	di, 1
	jnz	.negres
	jmp	.done

.negres:
	not	ax
	not	dx
	add	ax, 1
	adc	dx, 0

.done:
	pop	di
	pop	si
	pop	bx
	pop	bp
	ret
