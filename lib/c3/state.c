/*
 * Shared state between the two halves of c3.
 *
 * c3a (main.c + alloc.c) fills these arrays from the SSA stream and
 * serialises the allocation; c3b (gen.c + emit.c) reads the SSA again and
 * loads the allocation back into the same arrays.
 */
#include "c3.h"

struct insn	ins[MAXINS];
int		nins;
word		nval;
word		argpool[ARGPOOL];
int		nargpool;
byte		vdw[VDWBYTES];
word		locsize[MAXLOC];
int		nloc, nargs;

byte		busybits[MAXINS];
int		vslot[MAXVAL];
byte		vinfo[MAXVAL];
byte		vdir[MAXVAL];

int		framesize;
int		locsite[MAXLOC];
int		nsaves;
int		saved[NREG];

const int	addrreg[NREG] = { 0, 1, 0, 0, 1, 1 };
int		avoid[NREG];
