HOSTCC		?= cc
HOSTCFLAGS	+= -O2 -std=gnu99
HOSTLDFLAGS	+=
.EXPORTS: HOSTCC HOSTCFLAGS HOSTLDFLAGS

AS	?= nasm
AR	?= ia16-elf-ar
CC	?= ia16-elf-gcc
LD	?= ${CC}
OC	?= ia16-elf-objcopy
ASFLAGS	+=
CFLAGS	+= -Os -std=gnu99 -ffreestanding -masm=intel -march=i286 -mcmodel=small
CFLAGS	+= -Wall -Wextra -isystem $./lib/libc/include
LDFLAGS	+= -s -L${.OBJDIR}/$./lib/libc -nostdlib -lgcc -lc
LDFLAGS	+= -Wl,--no-warn-rwx-segments

.EXPORTS: AS AR CC LD OC ASFLAGS CFLAGS LDFLAGS
