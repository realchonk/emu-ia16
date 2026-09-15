[cpu 286]
[bits 16]

section .text
global _set_errno
extern errno
_set_errno:
	cmp	ax, 0
	jge	.ret

	neg	ax
	mov	word [errno], ax
	mov	ax, -1

.ret:
	xor dx, dx
	ret
