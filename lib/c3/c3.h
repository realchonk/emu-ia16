#ifndef FILE_C3_H
#define FILE_C3_H

#define SC_EXTERN	0
#define SC_GLOBAL	1
#define SC_STATIC	2

#ifndef MAXINS
#define MAXINS	1308	/* SSA instructions per function */
#endif
#ifndef MAXVAL
#define MAXVAL	1140	/* SSA values per function (must cover c1's NVALUE) */
#endif
#define MAXLOC	32	/* local (arg+var) slots */
#define ARGPOOL	384	/* shared pool for edge/call arguments */
#define VDWBYTES	((1140 + 7) / 8)	/* serialised value-dword bitmap */

typedef unsigned char	byte;
typedef unsigned short	word;
typedef unsigned long	dword;

/* register indices */
#define R_AX	0
#define R_BX	1
#define R_CX	2
#define R_DX	3
#define R_SI	4
#define R_DI	5
#define NREG	6

/* MDIR-only register numbers, outside the allocator's range */
#define R_SP	6
#define R_BP	7

/* segment register numbers */
#define SEG_ES	0
#define SEG_SS	2
#define SEG_DS	3

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
			word immhi;	/* high word (D) */
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
extern byte		vdw[];
extern word		locsize[];
extern int		nloc, nargs;

/* allocation state shared by c3a's allocator and c3b's emitter */
extern byte		busybits[];
extern int		vslot[];
extern byte		vinfo[];	/* [2:0] reg, [3] homed, [4] CL_ADDR */
#define VREG(v)		((vinfo[v]) & 7)
#define VHOMED(v)	((vinfo[v]) & 8)
#define VADDR(v)	((vinfo[v]) & 16)
extern byte		vdir[];
extern int		framesize;
extern int		locsite[];
extern int		nsaves;
extern int		saved[];
extern const int	addrreg[];
extern int		avoid[];

extern int		strfd, symfd;

/* value is a dword (kept in a frame slot) */
#define VDW(v)		(vdw[(v) >> 3] & (1 << ((v) & 7)))
/* call arguments carry a dword flag in the high bit of their pool word */
#define AVAL(w)		((w) & 0x7fff)
#define ADW(w)		((w) & 0x8000)

void	get ();
byte	getb ();
word	getw ();
dword	getd ();

/* buffered output (io.c) */
void	md_b ();
void	md_w ();
void	md_flush ();
void	raw ();

/* SSA reader (read.c) */
void	read_body ();

/* c3a */
void	alloc_func ();

/* c3b */
int	emit_func ();

#endif /* FILE_C3_H */
