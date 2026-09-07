## Directory Template
.template dir
.DEFAULT: all

## Build ${.SUBDIRS:J, }
all: ${.SUBDIRS}

clean: ${.SUBDIRS:=/clean}

.if target(all-extra)
all: all-extra
.endif

.if target(clean-extra)
clean: clean-extra
.endif

.endt


## Program Template
.template prog
.DEFAULT: all

BIN := ${NAME}
ARGS ?=

## Build ${NAME} program
all: ${BIN}

run: ${BIN} $./emu/emu
	$./emu/emu ${BIN:F} ${ARGS}

dump: ${NAME}.elf
	${OD} -ds -m i8086 -Mintel $< | bat -l asm

clean:
	rm -f ${BIN} *.o *.elf

${NAME}.elf: ${OBJS} $./lib/crt0.o $./lib/user.x $./lib/libc/libc.a
	${CC} -o $@ ${.OBJDIR}/$./lib/crt0.o ${OBJS:F} -T $./lib/user.x ${LDFLAGS}

${BIN}: ${NAME}.elf
	${OC} -O binary $< $@

.endt
