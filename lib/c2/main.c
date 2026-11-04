/*
 * c2 -- SSA peephole/optimizer between c1 and c3.
 *
 * Reads CIR\001 on stdin and writes an optimized CIR\001 stream on stdout.
 * The optimizations target code size:
 *
 *   - CSE of pure values (constants, string addresses, global/local
 *     addresses, casts, unary and binary expressions)
 *   - constant folding and algebraic identities
 *   - immediate operands for binary instructions
 *   - strength reduction of constant multiply/divide/remainder
 *   - store-to-load forwarding and overwritten-store removal
 *   - dead pure-instruction elimination
 *   - constant branches and redundant fall-through jumps
 *
 * No malloc; all pools are static, like c1/c3.
 */
#include <unistd.h>
#include <string.h>
#include <err.h>
#include "c2.h"

/* Keep the parser's small tables ahead of the big pools.  The optimizer
   has historically overrun a pool by one element; this placement keeps
   such a write out of the argument tables. */
static word	argsz[MAXLOC], varsz[MAXLOC];
static int	nargs, nvars;

struct insn	ins[MAXINS + 1];
int		nins;
word		nval;
word		argpool[ARGPOOL];
int		nargpool;
byte		vdw[(MAXVAL + 8) / 8];

byte		vsz[MAXVAL + 1];
word		kval[MAXVAL + 1];
word		ren[MAXVAL + 1];	/* rename map, then use counts */

/* ---- I/O -------------------------------------------------------------- */

void
get (buf, num)
void	*buf;
int	 num;
{
	int n;

	n = read (0, buf, num);
	if (n == num)
		return;
	if (n < 0)
		err (1, "c2: read()");
	errx (1, "c2: eof");
}

byte
getb ()
{
	byte b;
	get (&b, 1);
	return b;
}

word
getw ()
{
	word w;
	get (&w, 2);
	return w;
}

dword
getd ()
{
	dword d;
	get (&d, 4);
	return d;
}

static void
putb (b)
byte b;
{
	if (write (1, &b, 1) != 1)
		err (1, "c2: write()");
}

static void
putw (w)
word w;
{
	putb (w & 0xff);
	putb ((w >> 8) & 0xff);
}

static void
putd (d)
dword d;
{
	putw ((word) d);
	putw ((word) (d >> 16));
}

/* ---- small helpers ---------------------------------------------------- */

/* ---- data declarations (copied verbatim) ------------------------------ */

static void
do_def ()
{
	byte	flags, cnk;
	word	sym, size, a, b, n, i;

	flags = getb ();
	sym = getw ();
	size = getw ();
	putb ('D');
	putb (flags);
	putw (sym);
	putw (size);
	if (!(flags & 4))
		return;
	for (;;) {
		cnk = getb ();
		putb (cnk);
		if (cnk == 0)
			return;
		switch (cnk) {
		case 0x01:
			n = getw ();
			putw (n);
			for (i = 0; i < n; ++i)
				putb (getb ());
			break;
		case 0x02:
		case 0x03:
			a = getw ();
			b = getw ();
			putw (a);
			putw (b);
			break;
		default:
			errx (1, "c2: bad init chunk %#x", cnk);
		}
	}
}

/* ---- serialization ---------------------------------------------------- */

static void
put_insn (in)
struct insn *in;
{
	word	w;
	int	k;

	putb (in->op);
	switch (in->op) {
	case 'i':
		putb (in->v.i.size);
		putw (in->v.i.dst);
		if (in->v.i.size == SZ_B)
			putb (in->v.i.imm);
		else if (in->v.i.size == SZ_W)
			putw (in->v.i.imm);
		else
			putd (((dword) in->v.i.immhi << 16) | in->v.i.imm);
		break;
	case 'S':
		putw (in->v.S.dst);
		putw (in->v.S.six);
		break;
	case 'g':
		putw (in->v.g.dst);
		putw (in->v.g.sym);
		break;
	case 'l':
		putw (in->v.l.dst);
		putb (in->v.l.slot);
		break;
	case 'C':
		putb (in->v.C.fl);
		putw (in->v.C.dst);
		putw (in->v.C.a);
		break;
	case '*':
	case '=':
	case 'f':
	case 's':
		putb (in->v.m.size);
		putw (in->v.m.dst);
		putw (in->v.m.a);
		break;
	case 'p':
	case 'q':
		putw (in->v.g.dst);
		putw (in->v.g.sym);
		break;
	case 'u':
		putb (in->v.u.size);
		putb (in->v.u.bop);
		putw (in->v.u.dst);
		putw (in->v.u.a);
		break;
	case 'b':
		putb (in->v.b.fl);
		putb (in->v.b.bop);
		putw (in->v.b.dst);
		putw (in->v.b.a);
		putw (in->v.b.bi);
		break;
	case 'c':
		putb (in->v.c.size);
		putb (in->v.c.n);
		putw (in->v.c.dst);
		putw (in->v.c.a);
		for (k = 0; k < (int) in->v.c.n; ++k) {
			w = argpool[in->v.c.argi + k];
			putb (ADW (w) ? SZ_D : CSIZE (AVAL (w)));
			putw (AVAL (w));
		}
		break;
	case 'R':
		putw (in->v.R.a);
		break;
	case 'r':
		break;
	case 'L':
	case 'J':
		putw (in->v.L.lab);
		w = in->v.L.argn;
		putw (LN (w));
		for (k = 0; k < (int) LN (w); ++k)
			putw (argpool[LARGI (w) + k]);
		break;
	case 'B':
		putw (in->v.B.a);
		putw (in->v.B.lab);
		w = in->v.B.argn;
		putw (BN (w));
		for (k = 0; k < (int) BN (w); ++k)
			putw (argpool[BARGI (w) + k]);
		putw (in->v.B.lab2);
		putw (BNF (w));
		for (k = 0; k < (int) BNF (w); ++k)
			putw (argpool[BARGI (w) + BN (w) + k]);
		break;
	default:
		errx (1, "c2: cannot write %c", in->op);
	}
}

static void
read_simple (i)
int i;
{
	switch (ins[i].op) {
	case 'i':
		ins[i].v.i.size = getb ();
		ins[i].v.i.dst = getw ();
		ins[i].v.i.immhi = 0;
		if (ins[i].v.i.size == SZ_B)
			ins[i].v.i.imm = getb ();
		else if (ins[i].v.i.size == SZ_W)
			ins[i].v.i.imm = getw ();
		else {
			dword d = getd ();
			ins[i].v.i.imm = (word) d;
			ins[i].v.i.immhi = (word) (d >> 16);
		}
		break;
	case 'S':
		ins[i].v.S.dst = getw ();
		ins[i].v.S.six = getw ();
		break;
	case 'g':
	case 'p':
	case 'q':
		ins[i].v.g.dst = getw ();
		ins[i].v.g.sym = getw ();
		break;
	case 'l':
		ins[i].v.l.dst = getw ();
		ins[i].v.l.slot = getb ();
		break;
	case 'C':
		ins[i].v.C.fl = getb ();
		ins[i].v.C.dst = getw ();
		ins[i].v.C.a = getw ();
		break;
	case 'R':
		ins[i].v.R.a = getw ();
		break;
	case 'r':
		break;
	}
}

static void
read_mem (i)
int i;
{
	ins[i].v.m.size = getb ();
	ins[i].v.m.dst = getw ();
	ins[i].v.m.a = getw ();
}

static void
read_alu (i)
int i;
{
	if (ins[i].op == 'u') {
		ins[i].v.u.size = getb ();
		ins[i].v.u.bop = getb ();
		ins[i].v.u.dst = getw ();
		ins[i].v.u.a = getw ();
	} else {
		ins[i].v.b.fl = getb ();
		ins[i].v.b.bop = getb ();
		ins[i].v.b.dst = getw ();
		ins[i].v.b.a = getw ();
		ins[i].v.b.bi = getw ();
	}
}

static void
read_call (i)
int i;
{
	int k;
	word w;

	ins[i].v.c.size = getb ();
	ins[i].v.c.n = getb ();
	ins[i].v.c.dst = getw ();
	ins[i].v.c.a = getw ();
	ins[i].v.c.argi = nargpool;
	if (nargpool + (int) ins[i].v.c.n > ARGPOOL)
		errx (1, "c2: too many args");
	for (k = 0; k < (int) ins[i].v.c.n; ++k) {
		byte asz = getb ();
		w = getw ();
		argpool[nargpool++] = w | (asz == SZ_D ? 0x8000 : 0);
	}
}

static void
read_flow (i)
int i;
{
	int k;
	word w, n, nf, base;

	if (ins[i].op != 'B') {
		ins[i].v.L.lab = getw ();
		n = getw ();
		if (n > 15 || nargpool + n > ARGPOOL)
			errx (1, "c2: bad block args");
		ins[i].v.L.argn = (nargpool << 4) | n;
		for (k = 0; k < n; ++k)
			argpool[nargpool++] = getw ();
		return;
	}

	ins[i].v.B.a = getw ();
	ins[i].v.B.lab = getw ();
	n = getw ();
	if (n > 15 || nargpool + (int) n > ARGPOOL)
		errx (1, "c2: bad branch args");
	base = nargpool;
	ins[i].v.B.argn = (base << 7) | (n << 3);
	nargpool += n;
	for (k = 0; k < (int) n; ++k)
		argpool[base + k] = getw ();
	ins[i].v.B.lab2 = getw ();
	nf = getw ();
	if (nf > 7 || nargpool + (int) nf > ARGPOOL)
		errx (1, "c2: bad branch args");
	ins[i].v.B.argn |= nf;
	for (k = 0; k < (int) nf; ++k)
		argpool[base + n + k] = getw ();
	nargpool += nf;
	(void) w;
}

static void
read_insn (i)
int i;
{
	switch (ins[i].op) {
	case 'i':
	case 'S':
	case 'g':
	case 'l':
	case 'C':
	case 'p':
	case 'q':
	case 'R':
	case 'r':
		read_simple (i);
		break;
	case '*':
	case '=':
	case 'f':
	case 's':
		read_mem (i);
		break;
	case 'u':
	case 'b':
		read_alu (i);
		break;
	case 'c':
		read_call (i);
		break;
	case 'L':
	case 'J':
	case 'B':
		read_flow (i);
		break;
	default:
		errx (1, "c2: bad stmt %c", ins[i].op);
	}
}

static void
read_func ()
{
	int i;
	word w;

	nins = 0;
	nargpool = 0;
	nargs = 0;
	nvars = 0;

	/* args */
	while ((w = getw ()) != 0xffff)
		if (nargs < MAXLOC)
			argsz[nargs++] = w;
		else
			errx (1, "c2: too many locals");
	/* vars */
	while ((w = getw ()) != 0xffff)
		if (nvars < MAXLOC)
			varsz[nvars++] = w;
		else
			errx (1, "c2: too many locals");

	for (;;) {
		i = nins;
		if (i >= MAXINS)
			errx (1, "c2: too many instructions");
		ins[i].op = getb ();
		if (ins[i].op == 0xff)
			break;
		read_insn (i);
		++nins;
	}

	nval = 0;
	memset (vdw, 0, sizeof vdw);
	while ((w = getw ()) != 0xffff) {
		if (nval >= MAXVAL)
			errx (1, "c2: too many values");
		vsz[nval] = (w >> 14) & 3;
		if (vsz[nval] == SZ_D)
			vdw[nval >> 3] |= 1 << (nval & 7);
		++nval;
	}
	if (getb () != 0xff)
		errx (1, "c2: malformed F");

	optimize ();
}

/* ---- main ------------------------------------------------------------- */

static void
func (flags, sym)
int flags, sym;
{
	int	i;

	read_func ();

	putb ('F');
	putb (flags);
	putw (sym);
	for (i = 0; i < nargs; ++i)
		putw (argsz[i]);
	putw (0xffff);
	for (i = 0; i < nvars; ++i)
		putw (varsz[i]);
	putw (0xffff);
	for (i = 0; i < nins; ++i)
		if (ins[i].op != 0 && ins[i].op != 0xff)
			put_insn (&ins[i]);
	putb (0xff);
	for (i = 0; i < (int) nval; ++i)
		putw ((vsz[i] & 3) << 14);
	putw (0xffff);
	putb (0xff);
}

int
main ()
{
	byte type, flags;
	word sym;
	char magic[4];

	get (magic, 4);
	if (memcmp (magic, "CIR\001", 4) != 0)
		errx (1, "c2: invalid magic");
	putb ('C'); putb ('I'); putb ('R'); putb (1);
	while (read (0, &type, 1) == 1) {
		switch (type) {
		case 'D':
			do_def ();
			break;
		case 'F':
			flags = getb ();
			sym = getw ();
			if (!(flags & 4)) {
				putb ('F');
				putb (flags);
				putw (sym);
				putb (getb ());
			} else
				func (flags, sym);
			break;
		default:
			errx (1, "c2: bad global %c", type);
		}
	}
	return 0;
}
