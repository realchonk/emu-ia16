#ifndef FILE_C2_H
#define FILE_C2_H
#include "../c3/c3.h"

extern struct insn	ins[];
extern int		nins;
extern word		nval;
extern word		argpool[];
extern int		nargpool;
extern byte		vdw[];
extern byte		vsz[];
extern word		kval[];
extern word		ren[];

#define ISCONST(v)	(vsz[v] & 0x80)
#define CSIZE(v)	(vsz[v] & 3)

void	optimize ();
#endif
