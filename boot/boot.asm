[org 0x7c00]
[cpu 286]
[bits 16]

_start:
	mov sp, 0xf000

	; store the bootdisk
	mov byte [bootdisk], dl

	; copy self to 0x1000:0x7c00 to make space for the kernel
	mov ax, 0x1000
	mov es, ax
	xor ax, ax
	mov ds, ax
	mov si, 0x7c00
	mov di, si
	mov cx, 256
	rep movsw
	jmp 0x1000:.prompt

.prompt:
	lea si, [prompt]
	call puts

.wait:
	xor ah, ah
	int 0x16
	cmp al, 13
	jne .wait

	lea si, [nl]
	call puts

	; reset drive
	xor ah, ah
	int 0x13
	jc .error

	; read sectors
	xor ax, ax
	mov es, ax
	mov bx, 0x600	; load after the IVT and BDA
	mov ah, 0x02
	mov al, 125
	xor ch, ch
	mov cl, 2
	mov dl, byte [bootdisk]
	xor dh, dh
	int 0x13
	jc .error

	; jump into the kernel
	jmp 0:0x600

.error:
	lea si, [errstr]
	call puts

.halt:
	cli
	hlt
	jmp .halt

puts:
	mov al, byte [si]
	test al, al
	jz .ret

	inc si
	push si
	mov ah, 0x0e
	int 0x10
	pop si
	jmp puts

.ret:
	ret

bootdisk:
	db 0

prompt:
	db ": ", 0

nl:
	db 13, 10, 0

errstr:
	db "Error", 13, 10, 0

times 510 - ($ - $$) db 0
dw 0xAA55
