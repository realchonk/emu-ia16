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

.if !target(all)
## Build ${LIB}
all: ${LIB}
.endif

.if !target(install)
## Install ${LIB} into ${LIBDIR} and headers into ${INCDIR}
install: ${LIB}
	mkdir -p ${DESTDIR}${LIBDIR}
	cp -f ${LIB:F} ${DESTDIR}${PREFIX}/
.if defined(HDRDIR)
	mkdir -p ${DESTDIR}${INCDIR}
	cp -rf ${HDRDIR}/* ${DESTDIR}${INCDIR}/
.endif
.endif

.if !target(clean)
## Remove build artifacts
clean:
	rm -f ${LIB:F} *.o ${.SUBDIRS:=/*.o}
.endif

.if !target(dump)
## Disassemble ${LIB}
dump: ${LIB}
	${OD} -ds -m i8086 -Mintel $< | bat -l asm
.endif

.if !target(size)
## Compute the size for ${LIB}
size: ${LIB}
	${SIZE} -t $<
.endif

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

.if !target(all)
## Build ${NAME} program
all: ${BIN}
.endif

.if !target(install)
## Install ${NAME} into ${BINDIR}
install: ${BIN}
	mkdir -p ${DESTDIR}${BINDIR}
	cp -f ${BIN:F} ${DESTDIR}${BINDIR}/
.endif

.if !target(clean)
## Remove build artifacts
clean:
	rm -f ${BIN} *.o *.elf
.endif

.if !target(run)
## Run ${NAME} with $${ARGS}
run: ${BIN} ${EMU} ${RDEPS}
	${EMU:F} ${BIN:F} ${ARGS}
.endif

.if !target(dump)
## Disassemble ${NAME}
dump: ${NAME}.elf
	${OD} -ds -m i8086 -Mintel $< | bat -l asm
.endif

.if !target(size)
## Compute the size for ${NAME}
size: ${NAME}.elf
	${SIZE} $<
.endif

.if !target(${NAME}.elf)
${NAME}.elf: ${OBJS} ${LIBDEPS}
	${LD} -o $@ ${OBJS:F} ${LDFLAGS}
.endif

${BIN}: ${NAME}.elf
	${OC} -O binary $< $@

.expand extra
.endt
