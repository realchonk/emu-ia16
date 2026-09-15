[cpu 286]
[bits 16]

section .text
global _set_errnol
extern errno
_set_errnol:
	cmp	dx, 0
	jge	.ret

	neg	ax
	mov	word [errno], ax
	mov	ax, -1
	mov	dx, ax

.ret:
	ret
