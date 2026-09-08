/* Copyright (c) 2026 Robert Clausecker */
shim64:	push	%rdx
	movzwl	%ax, %edx	# get system call number (guest only sets %ax)
	call	sysentry
	pop	%rdx
	lretl
	.size	shim64, .-shim64

	/*
	 * 32 bit syscall entry shim.  This shim is copied to a fresh
	 * 32 bit segment and the hosts segment numbers are patched in
	 * at host_cs and host_ds.
	 */
	.code32
	.global	shim32, host_ds, host_cs, shim32_len
	.balign	16
shim32:	pushl	%ds
	pushl	%es
	push	%edi
	push	%esi
	.equ	host_ds, .+1
	mov	$0, %edi	# load 64-bit mode DS
	mov	%edi, %ds
	mov	%ss, %edi	# save current 16 bit stack
	lea	16(%esp), %esi
	lss	stack64, %esp	# switch to 64 bit mode stack
	push	%edi		# save 16 bit stack for later
	push	%esi
	.equ	host_cs, .+5
	lcalll	$0, $shim64
	pop	%esi
	pop	%edi
	mov	%edi, %ss	# restore 16 bit stack
	lea	-16(%esi), %esp
	pop	%esi
	pop	%edi
	popl	%es
	popl	%ds
	lretw			# return to 16 bit mode
	.equ	shim32_len, .-shim32
	.size	shim32, .-shim32
