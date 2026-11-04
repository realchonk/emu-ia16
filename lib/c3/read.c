/*
 * SSA (CIR\001) reader shared by both halves of c3.
 *
 * read_body() consumes the arg sizes, variable sizes, instructions and
 * value-size table of one function definition into the shared arrays.
 */
#include <string.h>
#include <err.h>
#include "c3.h"

static void
pool_args (n)
int n;
{
	if (nargpool + n > ARGPOOL)
		errx (1, "c3: too many pooled args");
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

	ins[i].v.c.size = getb ();
	ins[i].v.c.n = getb ();
	ins[i].v.c.dst = getw ();
	ins[i].v.c.a = getw ();
	ins[i].v.c.argi = nargpool;
	pool_args ((int) ins[i].v.c.n);
	for (k = 0; k < (int) ins[i].v.c.n; ++k) {
		byte asz = getb ();
		word av = getw ();
		argpool[nargpool++] = av | (asz == SZ_D ? 0x8000 : 0);
	}
}

static void
read_flow (i)
int i;
{
	int k;
	word n, base;

	ins[i].v.L.lab = getw ();
	n = getw ();
	if (n > 15)
		errx (1, "c3: too many block args");
	base = nargpool;
	pool_args ((int) n);
	ins[i].v.L.argn = (base << 4) | n;
	nargpool += n;
	for (k = 0; k < (int) n; ++k)
		argpool[base + k] = getw ();
}

static void
read_branch (i)
int i;
{
	int k;
	word n, nf, base;

	ins[i].v.B.a = getw ();
	ins[i].v.B.lab = getw ();
	n = getw ();
	if (n > 15)
		errx (1, "c3: too many block args");
	base = nargpool;
	pool_args ((int) n);
	nargpool += n;
	for (k = 0; k < (int) n; ++k)
		argpool[base + k] = getw ();
	ins[i].v.B.lab2 = getw ();
	nf = getw ();
	if (nf > 7)
		errx (1, "c3: too many block args");
	pool_args ((int) nf);
	nargpool += nf;
	for (k = 0; k < (int) nf; ++k)
		argpool[base + n + k] = getw ();
	ins[i].v.B.argn = (base << 7) | (n << 3) | nf;
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
		read_flow (i);
		break;
	case 'B':
		read_branch (i);
		break;
	default:
		errx (1, "c3: bad stmt %c (%#x)", ins[i].op, ins[i].op);
	}
}

void
read_body ()
{
	word w;
	int i;

	nloc = 0;
	nargs = 0;
	while ((w = getw ()) != 0xffff) {	/* arg sizes */
		if (nloc < MAXLOC) {
			locsize[nloc++] = w;
			++nargs;
		} else
			errx (1, "c3: too many locals");
	}
	while ((w = getw ()) != 0xffff) {	/* var sizes */
		if (nloc < MAXLOC)
			locsize[nloc++] = w;
		else
			errx (1, "c3: too many locals");
	}

	nins = 0;
	nargpool = 0;
	memset (vdw, 0, VDWBYTES);
	for (;;) {
		i = nins;
		if (i >= MAXINS)
			errx (1, "c3: too many instructions (%d used)", i);
		ins[i].op = getb ();
		if (ins[i].op == 0xff)
			break;
		read_insn (i);
		++nins;
	}

	nval = 0;
	while ((w = getw ()) != 0xffff) {
		if (nval >= MAXVAL)
			errx (1, "c3: too many values");
		if (((w >> 14) & 3) == SZ_D)
			vdw[nval >> 3] |= 1 << (nval & 7);
		++nval;
	}
	if (getb () != 0xff)
		errx (1, "c3: malformed F");
}
