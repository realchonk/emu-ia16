[bits 16]
[cpu 286]
[org 0]

xor ax, ax
mov bx, 0x0700 | 'U'
int 0x80

xor ax, ax
mov bx, 0x0700 | 'u'
int 0x80
jmp $
