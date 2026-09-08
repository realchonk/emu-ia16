HOSTCC		?= cc
HOSTCFLAGS	+= -O2 -std=gnu99
HOSTLDFLAGS	+=
.EXPORTS: HOSTCC HOSTCFLAGS HOSTLDFLAGS

AS	?= nasm
AR	?= ia16-elf-ar
CC	?= ia16-elf-gcc
LD	?= ${CC} ${.OBJDIR}/$./lib/crt0.o
OC	?= ia16-elf-objcopy
OD	?= ia16-elf-objdump
SIZE	?= ia16-elf-size
ASFLAGS	+=
CFLAGS	+= -Os -ansi -ffreestanding -masm=intel -march=i286
CFLAGS	+= -mcmodel=tiny -mprotected-mode
CFLAGS	+= -Wall -Wextra -isystem $./lib/libc/include
LDFLAGS	+= -L${.OBJDIR}/$./lib/libc -nostdlib -lc -lgcc
LDFLAGS	+= -T $./lib/user.x -Wl,--no-warn-rwx-segments

.EXPORTS: AS AR CC LD OC OD SIZE ASFLAGS CFLAGS LDFLAGS
