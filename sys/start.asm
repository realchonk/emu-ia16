[bits 16]
[cpu 286]

section .boot
global _start
_start:
	mov ax, 0xb800
	mov es, ax
	mov byte [es:0], 'X'

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
	mov ax, 0x18
	mov es, ax
	mov byte [es:0], 'Y'

_halt:
	cli
	hlt
	jmp _start

section .rodata
gdtr:
	dw gdt.end - gdt - 1
	dw gdt
	dw 0

idtr:
	dw idt.end - idt - 1
	dw idt
	dw 0

section .bss
idt:
	resq 256
.end:

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
	
.end:
	
