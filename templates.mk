## "extra" rules
.template extra
.if target(all-extra)
all: all-extra
.endif

.if target(clean-extra)
clean: clean-extra
.endif

.if target(install-extra)
install: install-extra
.endif
.endt

## Directory Template
.template dir
.DEFAULT: all

## Build ${.SUBDIRS:J, }
all: ${.SUBDIRS}

## Install ${.SUBDIRS:J, }
install: ${.SUBDIRS:=/install}

## Clean ${.SUBDIRS:J, }
clean: ${.SUBDIRS:=/clean}

.expand extra
.endt

## Library Template
.template lib
.DEFAULT: all

LIB := lib${NAME}.a

## Build ${LIB}
all: ${LIB}

## Install ${LIB} into ${PREFIX}/lib
install: ${LIB}
	mkdir -p ${DESTDIR}${PREFIX}/lib
	cp -f ${LIB:F} ${DESTDIR}${PREFIX}/lib/

## Remove build artifacts
clean:
	rm -f ${LIB:F} *.o ${.SUBDIRS:=/*.o}

## Disassemble ${LIB}
dump: ${LIB}
	${OD} -ds -m i8086 -Mintel $< | bat -l asm

## Compute the size for ${LIB}
size: ${LIB}
	${SIZE} -t $<

${LIB}: ${OBJS}
	${AR} rcs $@ ${OBJS:F}

.expand extra
.endt

## Program Template
.template prog
.DEFAULT: all

BIN := ${NAME}

## Program Arguments for `run`
ARGS ?=

## Build ${NAME} program
all: ${BIN}

## Install ${NAME} into ${PREFIX}/bin
install: ${BIN}
	mkdir -p ${DESTDIR}${PREFIX}/bin
	cp -f ${BIN:F} ${DESTDIR}${PREFIX}/bin/

## Remove build artifacts
clean:
	rm -f ${BIN} *.o *.elf

.if !target(run)
## Run ${NAME} with $${ARGS}
run: ${BIN} ${EMU}
	${EMU:F} ${BIN:F} ${ARGS}
.endif

## Disassemble ${NAME}
dump: ${NAME}.elf
	${OD} -ds -m i8086 -Mintel $< | bat -l asm

## Compute the size for ${NAME}
size: ${NAME}.elf
	${SIZE} $<

.if !target(${NAME}.elf)
${NAME}.elf: ${OBJS} ${LIBDEPS}
	${LD} -o $@ ${OBJS:F} ${LDFLAGS}
.endif

${BIN}: ${NAME}.elf
	${OC} -O binary $< $@

.expand extra
.endt
