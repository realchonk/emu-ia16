[bits 16]
[cpu 286]

section .boot
extern main, __bss_begin, __bss_end, __bss_size
global _start
_start:
	mov ax, 0xb800
	mov es, ax
	mov byte [es:0], 'X'

	; clear .bss
	lea di, [__bss_begin]
	mov cx, __bss_size
	xor ax, ax
	mov es, ax
	rep stosb

	; enable A20 via BIOS
	mov ax, 0x2401
	int 0x15
	jc _halt

	cli
	lgdt [gdtr]

	; enable protected mode
	smsw ax
	or ax, 1
	lmsw ax

	mov ax, 0x10
	mov ds, ax
	mov es, ax
	mov ss, ax
	jmp 0x08:_pm

_pm:
	lidt [idtr]

	lea sp, [stack]
	call main


	mov ax, 0x18
	mov es, ax
	mov byte [es:0], 'Z'

_halt:
	cli
	hlt
	jmp _start

; void setldt (void *ldt, size_t size);
global setldt
setldt:
	push bp
	mov bp, sp

	mov ax, word [bp + 6]	; size
	mov word [gdt.ldt + 0], ax

	mov ax, word [bp + 4]	; ldt
	mov word [gdt.ldt + 2], ax

	; reload LDTR
	mov ax, 0x20
	lldt ax

	mov sp, bp
	pop bp
	ret

; void putldt (void *ldt, int offset, uint32_t base, size_t size, int access);
global putldt
putldt:
	push bp
	mov bp, sp
	
	mov bx, word [bp + 4]
	add bx, word [bp + 6]	; offset

	mov ax, word [bp + 12]	; size
	mov word [bx + 0], ax
	
	mov ax, word [bp + 8]	; base[15:0]
	mov word [bx + 2], ax
	mov ax, word [bp + 10]	; base[31:16]
	mov byte [bx + 4], al

	mov ax, word [bp + 14]	; access
	mov byte [bx + 5], al

	mov word [bx + 6], 0	; reserved for 386+

	mov sp, bp
	pop bp
	ret

; void puttss (void *stack);
global puttss
puttss:
	push bp
	mov bp, sp

	mov ax, word [bp + 4]	; stack
	mov word [tss + 2], ax

	; reload TR
	mov ax, 0x28
	ltr ax

	mov sp, bp
	pop bp
	ret

; void putidt (int offset, int cs, void *base, int type);
global putidt
putidt:
	push bp
	mov bp, sp

	mov bx, word [bp + 4]	; offset
	add bx, idt

	mov ax, word [bp + 8]	; base
	mov word [bx + 0], ax
	
	mov ax, word [bp + 6]	; cs
	mov word [bx + 2], ax

	mov byte [bx + 4], 0	; reserved
	mov ax, word [bp + 10]	; type
	mov byte [bx + 5], al
	mov word [bx + 6], 0	; reserved for 386+

	lidt [idtr]

	mov sp, bp
	pop bp
	ret

; __dead void enter (int cs, int ip, int ss, int sp);
global enter
enter:
	mov bp, sp

	mov ax, word [bp + 2]	; cs
	mov bx, word [bp + 4]	; ip
	mov cx, word [bp + 6]	; ss
	mov dx, word [bp + 8]	; sp

	mov ds, cx
	mov es, cx

	push cx		; ss
	push dx		; sp
	push 0x002	; flags
	push ax		; cs
	push bx		; ip
	iret

global i_sys
extern syscall
i_sys:
	push dx
	push cx
	push bx
	push ax
	call syscall
	add sp, 8
	iret

section .rodata
gdtr:
	dw gdt.end - gdt - 1
	dw gdt
	dw 0

idtr:
	dw idt.end - idt - 1
	dw idt
	dw 0

section .data
tss:
	dw 0x0000	; link
	dw 0x0000	; sp
	dw 0x0010	; ss0
.end:

section .bss
global stack
idt:
	resq 256
.end:
	resb 256
stack:

section .data
gdt:
.null:			; 0x00
	dq 0

.text:			; 0x08
	dw 0xffff	; limit
	dw 0x0000	; base[15:0]
	db 0x00		; base[23:16]
	db 0x9A		; RX
	dw 0x0000	; reserved for 386+

.data:			; 0x10
	dw 0xffff	; limit
	dw 0x0000	; base[15:0]
	db 0x00		; base[23:16]
	db 0x92		; RW
	dw 0x0000	; reserved for 386+

.video:			; 0x18
	dw (80 * 25 * 2)
	dw 0x8000	; base[15:0]
	db 0x0b		; base[23:16]
	db 0x92		; RW
	dw 0x0000	; reserved for 386+

.ldt:			; 0x20
	dw 0x0000	; limit
	dw 0x0000	; base[15:0]
	db 0x00		; base[23:16]
	db 0x82		; LDT
	dw 0x0000	; reserved for 386+

.tss:			; 0x28
	dw tss.end-tss	; limit
	dw tss		; base[15:0]
	db 0x00		; base[23:16]
	db 0x81		; TSS
	dw 0x0000	; reserved for 386+
	
.end:
	
