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

size: ${NAME}.elf
	${SIZE} $<

clean:
	rm -f ${BIN} *.o *.elf

.if !target(${NAME}.elf)
${NAME}.elf: ${OBJS} ${LIBDEPS}
	${LD} -o $@ ${OBJS:F} ${LDFLAGS}
.endif

${BIN}: ${NAME}.elf
	${OC} -O binary $< $@

.endt
