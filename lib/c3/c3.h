#ifndef FILE_C3_H
#define FILE_C3_H

#define SC_EXTERN	0
#define SC_GLOBAL	1
#define SC_STATIC	2

#define MAXINS	2680	/* SSA instructions per function */
#define MAXVAL	2400	/* SSA values per function (must cover c1's NVALUE) */
#define MAXLOC	32	/* local (arg+var) slots */
#define ARGPOOL	512	/* shared pool for edge/call arguments */

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
   active member is `v.i`, `v.S`, `v.b`, ... selected by `op`.

   The payload structs are hand-budgeted to 8 bytes so an insn costs 10
   bytes (op + pad + payload): `i` keeps only the low word of a dword
   immediate (the backend emits word moves), `b`'s operand `bi` holds
   either the second value id or the immediate (selected by fl&8), and
   the L/J/B arg-list descriptors are packed into one word:

	L.argn  = argi << 4 | n                 (n <= 15)
	B.argn  = argi << 7 | n << 3 | nfalse   (n <= 15, nfalse <= 7,
	                                         fargi == argi + n)
*/
struct insn {
	byte op;		/* 'i','S','g','l','C','*','=','u','b',
				   'c','R','r','L','J','B' */
	union {
		struct {		/* 'i': integer constant */
			byte size;	/* SZ_B/W/D */
			word dst;	/* result value */
			word imm;	/* value (B/W), low word (D) */
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
			byte slot;
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
		struct {		/* 'b': binary; size == fl & 3 */
			byte fl, bop;
			word dst, a, bi;
		} b;
		struct {		/* 'c': call */
			byte size, n;
			word dst, a, argi;
		} c;
		struct {		/* 'R': return value */
			word a;
		} R;
		struct {		/* 'L' / 'J': label / jump */
			word lab, argn;
		} L;
		struct {		/* 'B': branch */
			word a;		/* condition */
			word lab, lab2;
			word argn;	/* true edge, false edge at argi + n */
		} B;
	} v;
};

/* argn unpacking */
#define LARGI(w)	(((w) >> 4))
#define LN(w)		(((w) & 15))
#define BARGI(w)	(((w) >> 7))
#define BN(w)		(((w) >> 3) & 15)
#define BNF(w)		(((w) & 7))

extern struct insn	ins[];
extern int		nins;
extern word		nval;
extern word		argpool[];
extern int		nargpool;

void	get ();
byte	getb ();
word	getw ();
dword	getd ();
const char *getsym ();

int	emit_func ();

#endif /* FILE_C3_H */
