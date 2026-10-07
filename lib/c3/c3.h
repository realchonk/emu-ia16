#ifndef FILE_C3_H
#define FILE_C3_H

#define SC_EXTERN	0
#define SC_GLOBAL	1
#define SC_STATIC	2

#define MAXINS	1700	/* SSA instructions per function */
#define MAXVAL	1200	/* SSA values per function */
#define MAXARGS	8	/* block / call args */
#define MAXLOC	48	/* local (arg+var) slots */
#define ARGPOOL	3072	/* shared pool for edge/call arguments */

typedef unsigned char	byte;
typedef unsigned short	word;
typedef unsigned long	dword;

extern int strfd, symfd;
extern char funcname[];

/* register indices */
#define R_AX	0
#define R_BX	1
#define R_CX	2
#define R_DX	3
#define R_SI	4
#define R_DI	5
#define NREG	6

/* SSA size codes */
#define SZ_B	0
#define SZ_W	1
#define SZ_D	2

/* value classes */
#define CL_VAL	0
#define CL_ADDR	1

/* Decoded SSA instruction.  Like c1's struct expr/stmt, only `op` is
   common; every other field lives in a union of per-opcode payload structs,
   so the fixed ins[] array stays small enough for the 64K segment.  The
   active member is `v.i`, `v.S`, `v.b`, ... selected by `op`. */
struct insn {
	byte op;		/* 'i','I','S','g','l','C','*','=','u','b',
				   'c','R','r','L','J','B' */
	union {
		struct {		/* 'i': integer constant */
			byte size;	/* SZ_B/W/D */
			word dst;	/* result value */
			word imm;	/* low word (size B/W) */
			dword immd;	/* full value (size D) */
		} i;
		struct {		/* 'S': string literal */
			word dst;
			word six;
		} S;
		struct {		/* 'g': global address */
			word dst;
			word sym;
		} g;
		struct {		/* 'l': local address */
			word dst;
			word slot;
		} l;
		struct {		/* 'C': cast */
			byte fl;
			word dst, a;
		} C;
		struct {		/* '*' and '=': load/store */
			byte size;
			word dst, a;
		} m;
		struct {		/* 'u': unary */
			byte size, bop;
			word dst, a;
		} u;
		struct {		/* 'b': binary */
			byte size, fl, bop;
			word dst, a, b, imm;
		} b;
		struct {		/* 'c': call */
			byte size, n;
			word dst, a, argi;
		} c;
		struct {		/* 'R': return value */
			word a;
		} R;
		struct {		/* 'L' / 'J': label (params) / jump (args) */
			word lab, n, argi;
		} L;
		struct {		/* 'B': branch */
			word a;		/* condition */
			word lab, n, argi;	/* true edge */
			word lab2, nfalse, fargi;	/* false edge */
		} B;
	} v;
};

/* home of a value */
struct home {
	byte	 kind;		/* 0 = register, 1 = frame slot */
	byte	 reg;
	int	 off;
};

extern struct insn	ins[];
extern int		nins;
extern word		nval;
extern byte		vsz[];
extern word		argpool[];
extern int		nargpool;

void	get ();
byte	getb ();
word	getw ();
dword	getd ();
const char *getsym ();

int	emit_func ();

#endif /* FILE_C3_H */
