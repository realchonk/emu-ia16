/*
 * emit.c -- register allocation and code generation for c3.
 *
 * Allocation is a linear scan over conservative live intervals: instructions
 * are numbered in source order (RPO for c0's output) and a value's interval
 * is [first def, last use].  A value live across a call is kept in a
 * callee-saved register (SI/DI); spilled values go to bp-relative slots and
 * are reloaded into a scratch chosen from the registers not live there.
 *
 * Register assignment is fixed for a value's whole live range, so no
 * re-materialisation is needed.  Block arguments (the SSA replacement for
 * phi nodes) become explicit moves on each edge; c0 only produces
 * single-parameter merges on acyclic edges, so sequential moves suffice.
 */
#include <string.h>
#include <unistd.h>
#include <stdlib.h>
#include <stdio.h>
#include <err.h>
#include "c3.h"

extern struct insn	ins[];
extern int		nins;
extern word		nval;
extern word		argpool[];
extern int		nargpool;
extern word		locsize[];
extern int		nloc, nargs;

static int	ivlo[MAXVAL], ivhi[MAXVAL];
static int	vslot[MAXVAL];
static byte	vinfo[MAXVAL];	/* [2:0] reg, [3] homed, [4] CL_ADDR */

#define VREG(v)		((vinfo[v]) & 7)
#define VHOMED(v)	((vinfo[v]) & 8)
#define VADDR(v)	((vinfo[v]) & 16)

static int	framesize;
static int	locsite[MAXLOC];
static int	nsaves;
static int	saved[NREG];

static const char *regname[] = { "ax", "bx", "cx", "dx", "si", "di" };
static const char *bytename[] = { "al", "bl", "cl", "dl" };
static const int addrreg[NREG] = { 0, 1, 0, 0, 1, 1 };

static int	avoid[NREG];
static word	funcsym;		/* current function symbol, for local labels */

static void
touch (v, lo, hi)
word v;
int lo, hi;
{
	if (v >= nval)
		return;
	if (lo >= 0 && ivlo[v] > lo)
		ivlo[v] = lo;
	if (hi >= 0 && ivhi[v] < hi)
		ivhi[v] = hi;
}

static void
compute_intervals ()
{
	int i, k;
	word v;

	for (v = 0; v < nval; ++v) {
		ivlo[v] = 0x7fff;
		ivhi[v] = -1;
		vinfo[v] = 0;
	}

	for (i = 0; i < nins; ++i) {
		switch (ins[i].op) {
		case 'i':
			touch (ins[i].v.i.dst, i, i);
			break;
		case 'S': case 'g': case 'l':
			touch (ins[i].v.S.dst, i, i);
			break;
		case 'c':
			touch (ins[i].v.c.dst, i, i);
			touch (ins[i].v.c.a, 0, i);
			for (k = 0; k < ins[i].v.c.n; ++k)
				touch (argpool[ins[i].v.c.argi + k], 0, i);
			break;
		case 'C':
			touch (ins[i].v.C.dst, i, i);
			touch (ins[i].v.C.a, 0, i);
			vinfo[ins[i].v.C.dst] = vinfo[ins[i].v.C.a] & 0x10;
			break;
		case '*':
			touch (ins[i].v.m.dst, i, i);
			touch (ins[i].v.m.a, 0, i);
			vinfo[ins[i].v.m.a] |= 16;
			break;
		case '=':
			touch (ins[i].v.m.dst, 0, i);
			touch (ins[i].v.m.a, 0, i);
			vinfo[ins[i].v.m.dst] |= 16;
			break;
		case 'u':
			touch (ins[i].v.u.dst, i, i);
			touch (ins[i].v.u.a, 0, i);
			break;
		case 'b':
			touch (ins[i].v.b.dst, i, i);
			touch (ins[i].v.b.a, 0, i);
			if (!(ins[i].v.b.fl & 8))
				touch (ins[i].v.b.bi, 0, i);
			break;
		case 'R':
			touch (ins[i].v.R.a, 0, i);
			break;
		case 'J':
			for (k = 0; k < (int) LN (ins[i].v.L.argn); ++k)
				touch (argpool[LARGI (ins[i].v.L.argn) + k], 0, i);
			break;
		case 'B':
			touch (ins[i].v.B.a, 0, i);
			for (k = 0; k < (int) BN (ins[i].v.B.argn); ++k)
				touch (argpool[BARGI (ins[i].v.B.argn) + k], 0, i);
			for (k = 0; k < (int) BNF (ins[i].v.B.argn); ++k)
				touch (argpool[BARGI (ins[i].v.B.argn)
				    + BN (ins[i].v.B.argn) + k], 0, i);
			break;
		case 'L':
			for (k = 0; k < (int) LN (ins[i].v.L.argn); ++k)
				touch (argpool[LARGI (ins[i].v.L.argn) + k], i, 0x7fff);
			break;
		}
	}
	for (v = 0; v < nval; ++v)
		if (ivlo[v] > ivhi[v])
			ivlo[v] = ivhi[v] = 0;
}

static int
crosses_call (v)
word v;
{
	int i;

	for (i = 0; i < nins; ++i)
		if (ins[i].op == 'c' && ivlo[v] <= i && i <= ivhi[v])
			return 1;
	return 0;
}

static int
allowed (v, r)
word v;
int r;
{
	if (VADDR (v) && !addrreg[r])
		return 0;
	return 1;
}

static void
allocate ()
{
	int i, j, t, cand, victim, vend, norder, bestlo;
	word v;
	int owner[NREG];
	word active[NREG];		/* an active value owns a register */
	int nactive;

	/* Values are visited by increasing ivlo (ties by id, like a stable
	   insertion sort), marked with vinfo bit 5; there is no room for an
	   order[] pool, and active[] never holds more than NREG values. */
	norder = 0;
	for (v = 0; v < nval; ++v) {
		vinfo[v] &= ~32;
		if (ivhi[v] >= 0)
			++norder;
	}

	for (i = 0; i < NREG; ++i)
		owner[i] = -1;
	nactive = 0;

	while (norder > 0) {
		v = 0;
		bestlo = 0x7fff;
		for (j = 0; j < (int) nval; ++j)
			if (!(vinfo[j] & 32) && ivhi[j] >= 0 && ivlo[j] < bestlo) {
				bestlo = ivlo[j];
				v = j;
			}
		vinfo[v] |= 32;
		--norder;

		for (j = 0; j < nactive; ) {
			if (ivhi[active[j]] < ivlo[v]) {
				owner[VREG (active[j])] = -1;
				active[j] = active[--nactive];
			} else
				++j;
		}

		cand = -1;
		for (t = 0; t < NREG; ++t) {
			if (!allowed (v, t) || owner[t] != -1)
				continue;
			if (crosses_call (v) && t != R_SI && t != R_DI)
				continue;
			cand = t;
			break;
		}

		if (cand < 0) {
			victim = -1;
			vend = -1;
			for (j = 0; j < nactive; ++j) {
				t = active[j];
				if (!allowed (v, VREG (t)))
					continue;
				if (ivhi[t] > vend) {
					vend = ivhi[t];
					victim = j;
				}
			}
			if (victim >= 0 && ivhi[active[victim]] > ivhi[v]) {
				word av = active[victim];
				vinfo[av] |= 8;
				owner[VREG (av)] = -1;
				active[victim] = active[--nactive];
				cand = VREG (av);
			} else {
				vinfo[v] |= 8;
				continue;
			}
		}
		if (cand < 0)
			cand = R_AX;
		vinfo[v] = (vinfo[v] & 0x30) | cand;
		owner[cand] = v;
		if (nactive >= NREG)
			errx (1, "c3: allocator invariant broken");
		active[nactive++] = v;
	}

	/* frame: saved SI/DI occupy [bp-2],[bp-4]; slots sit below them */
	framesize = 4;
	for (i = 0; i < nloc; ++i) {
		int sz = locsize[i] < 2 ? 2 : (locsize[i] >= 4 ? 4 : 2);
		locsite[i] = -(framesize + 2);
		framesize += sz;
	}
	j = 0;
	for (v = 0; v < nval; ++v)
		if (VHOMED (v)) {
			framesize += 2;
			vslot[v] = -framesize;
			++j;
		}
	if (framesize & 1)
		framesize++;
}

static int
live_at (v, pc)
word v;
int pc;
{
	return ivlo[v] <= pc && pc <= ivhi[v];
}

static int
busy (pc, r)
int pc, r;
{
	word v;

	if (avoid[r])
		return 1;
	for (v = 0; v < nval; ++v)
		if (!VHOMED (v) && VREG (v) == r && live_at (v, pc))
			return 1;
	return 0;
}

static int
scratch (pc, wantaddr)
int pc, wantaddr;
{
	int r;

	for (r = 0; r < NREG; ++r)
		if ((!wantaddr || addrreg[r]) && !busy (pc, r)) {
			avoid[r] = 1;
			return r;
		}
	for (r = 0; r < NREG; ++r)
		if (!wantaddr || addrreg[r]) {
			avoid[r] = 1;
			return r;
		}
	return R_AX;
}

static void
readop (v, out, pc)
word v;
char *out;
int pc;
{
	int r;

	if (!VHOMED (v)) {
		sprintf (out, "%s", regname[VREG (v)]);
		return;
	}
	r = scratch (pc, VADDR (v) != 0);
	printf ("\tmov %s, [bp%+d]\n", regname[r], vslot[v]);
	sprintf (out, "%s", regname[r]);
}

static void
memop (v, out, pc)
word v;
char *out;
int pc;
{
	int r;

	if (!VHOMED (v) && addrreg[VREG (v)]) {
		sprintf (out, "[%s]", regname[VREG (v)]);
		return;
	}
	r = scratch (pc, 1);
	if (!VHOMED (v))
		printf ("\tmov %s, %s\n", regname[r], regname[VREG (v)]);
	else
		printf ("\tmov %s, [bp%+d]\n", regname[r], vslot[v]);
	sprintf (out, "[%s]", regname[r]);
}

static void
toreg (v, r, pc)
word v;
int r;
int pc;
{
	(void) pc;
	if (!VHOMED (v)) {
		if (VREG (v) != r)
			printf ("\tmov %s, %s\n", regname[r], regname[VREG (v)]);
	} else
		printf ("\tmov %s, [bp%+d]\n", regname[r], vslot[v]);
}

static void
stohome (v, r, pc)
word v;
int r;
int pc;
{
	(void) pc;
	if (VHOMED (v))
		printf ("\tmov [bp%+d], %s\n", vslot[v], regname[r]);
	else if (VREG (v) != r)
		printf ("\tmov %s, %s\n", regname[VREG (v)], regname[r]);
}

static struct insn *
find_label (lab)
word lab;
{
	int i;

	for (i = 0; i < nins; ++i)
		if (ins[i].op == 'L' && ins[i].v.L.lab == lab)
			return &ins[i];
	return (struct insn *) 0;
}

static void
edge (argsbase, nargc, lab, pc)
int argsbase, nargc;
word lab;
int pc;
{
	struct insn *li;
	int k, np;
	word src, dst;
	char a[32];

	li = find_label (lab);
	if (li == (struct insn *) 0)
		return;
	np = LN (li->v.L.argn);
	/* c0's merges pass at most one parameter on acyclic edges, so a
	   straightforward sequential move is safe here. */
	for (k = 0; k < np && k < nargc; ++k) {
		src = argpool[argsbase + k];
		dst = argpool[LARGI (li->v.L.argn) + k];
		if (src == dst)
			continue;
		readop (src, a, pc);
		if (VHOMED (dst))
			printf ("\tmov [bp%+d], %s\n", vslot[dst], a);
		else if (strcmp (a, regname[VREG (dst)]) != 0)
			printf ("\tmov %s, %s\n", regname[VREG (dst)], a);
	}
}

static const char *jcc[] = { "jb", "jbe", "ja", "jae", "je", "jne" };

static const char *
bopname (op)
int op;
{
	switch (op) {
	case '+': return "add";
	case '-': return "sub";
	case '*': return "mul";
	case '/': return "div";
	case '%': return "mod";
	case '&': return "and";
	case '|': return "or";
	case '^': return "xor";
	}
	return "?";
}

static int
cmpidx (op)
int op;
{
	switch (op) {
	case '<': return 0;
	case 'L': return 1;
	case '>': return 2;
	case 'G': return 3;
	case 'E': return 4;
	case 'N': return 5;
	}
	return -1;
}

static void
gen (in, pc)
struct insn *in;
int pc;
{
	char a[32], b[32], m[32];
	int r, i, t;

	for (i = 0; i < NREG; ++i)
		avoid[i] = 0;

	switch (in->op) {
	case 'i':
		if (VHOMED (in->v.i.dst))
			printf ("\tmov word [bp%+d], %u\n", vslot[in->v.i.dst],
				(unsigned) in->v.i.imm);
		else
			printf ("\tmov %s, %u\n", regname[VREG (in->v.i.dst)],
				(unsigned) in->v.i.imm);
		break;
	case 'S':
		if (VHOMED (in->v.S.dst))
			printf ("\tmov word [bp%+d], strtab + %u\n",
				vslot[in->v.S.dst], (unsigned) in->v.S.six);
		else
			printf ("\tmov %s, strtab + %u\n",
				regname[VREG (in->v.S.dst)], (unsigned) in->v.S.six);
		break;
	case 'g':
		if (VHOMED (in->v.g.dst))
			printf ("\tmov word [bp%+d], %s\n",
				vslot[in->v.g.dst], getsym (in->v.g.sym));
		else
			printf ("\tmov %s, %s\n", regname[VREG (in->v.g.dst)],
				getsym (in->v.g.sym));
		break;
	case 'l':
		if (!VHOMED (in->v.l.dst)) {
			printf ("\tlea %s, [bp%+d]\n",
				regname[VREG (in->v.l.dst)], locsite[in->v.l.slot]);
		} else {
			t = scratch (pc, 1);
			printf ("\tlea %s, [bp%+d]\n", regname[t],
				locsite[in->v.l.slot]);
			printf ("\tmov [bp%+d], %s\n", vslot[in->v.l.dst],
				regname[t]);
		}
		break;
	case 'C':
		toreg (in->v.C.a, R_AX, pc);
		stohome (in->v.C.dst, R_AX, pc);
		break;
	case '*':
		memop (in->v.m.a, m, pc);
		if (in->v.m.size == SZ_B) {
			r = !VHOMED (in->v.m.dst) ? VREG (in->v.m.dst)
						    : scratch (pc, 0);
			if (r == R_SI || r == R_DI) {
				printf ("\tmov ax, 0\n");
				printf ("\tmov al, %s\n", m);
				stohome (in->v.m.dst, R_AX, pc);
			} else {
				printf ("\tmov %s, 0\n", regname[r]);
				printf ("\tmov %s, %s\n", bytename[r], m);
				if (VHOMED (in->v.m.dst))
					printf ("\tmov [bp%+d], %s\n",
						vslot[in->v.m.dst], regname[r]);
			}
		} else {
			r = !VHOMED (in->v.m.dst) ? VREG (in->v.m.dst)
						    : scratch (pc, 0);
			printf ("\tmov %s, %s\n", regname[r], m);
			if (VHOMED (in->v.m.dst))
				printf ("\tmov [bp%+d], %s\n",
					vslot[in->v.m.dst], regname[r]);
		}
		break;
	case '=':
		if (in->v.m.size == SZ_B) {
			int ss = scratch (pc, 0);
			readop (in->v.m.a, a, pc);
			printf ("\tmov %s, %s\n", regname[ss], a);
			memop (in->v.m.dst, m, pc);
			printf ("\tmov %s, %s\n", m, bytename[ss]);
		} else {
			readop (in->v.m.a, a, pc);
			memop (in->v.m.dst, m, pc);
			printf ("\tmov %s, %s\n", m, a);
		}
		break;
	case 'u':
		toreg (in->v.u.a, R_AX, pc);
		if (in->v.u.bop == '-')
			printf ("\tneg ax\n");
		else if (in->v.u.bop == '~')
			printf ("\tnot ax\n");
		else {
			printf ("\ttest ax, ax\n");
			printf ("\tmov ax, 0\n");
			printf ("\tjnz .Lnot%u\n", (unsigned) pc);
			printf ("\tmov ax, 1\n");
			printf (".Lnot%u:\n", (unsigned) pc);
		}
		stohome (in->v.u.dst, R_AX, pc);
		break;
	case 'b':
		r = cmpidx (in->v.b.bop);
		if (r >= 0) {
			readop (in->v.b.a, a, pc);
			if (in->v.b.fl & 8)
				sprintf (b, "%u", (unsigned) in->v.b.bi);
			else
				readop (in->v.b.bi, b, pc);
			printf ("\tcmp %s, %s\n", a, b);
			printf ("\t%s .Lset%u\n", jcc[r], (unsigned) pc);
			if (VHOMED (in->v.b.dst))
				printf ("\tmov word [bp%+d], 0\n",
					vslot[in->v.b.dst]);
			else
				printf ("\tmov %s, 0\n",
					regname[VREG (in->v.b.dst)]);
			printf ("\tjmp .Lendc%u\n", (unsigned) pc);
			printf (".Lset%u:\n", (unsigned) pc);
			if (VHOMED (in->v.b.dst))
				printf ("\tmov word [bp%+d], 1\n",
					vslot[in->v.b.dst]);
			else
				printf ("\tmov %s, 1\n",
					regname[VREG (in->v.b.dst)]);
			printf (".Lendc%u:\n", (unsigned) pc);
			break;
		}
		if (in->v.b.bop == '*') {
			if (!VHOMED (in->v.b.bi) && VREG (in->v.b.bi) != R_AX &&
			    VREG (in->v.b.bi) != R_DX) {
				sprintf (b, "%s", regname[VREG (in->v.b.bi)]);
			} else {
				t = scratch (pc, 0);
				if (t == R_AX || t == R_DX)
					t = R_BX;
				readop (in->v.b.bi, a, pc);
				printf ("\tmov %s, %s\n", regname[t], a);
				sprintf (b, "%s", regname[t]);
			}
			toreg (in->v.b.a, R_AX, pc);
			printf ("\tmul %s\n", b);
			stohome (in->v.b.dst, R_AX, pc);
			break;
		}
		if (in->v.b.bop == '/' || in->v.b.bop == '%') {
			if (!VHOMED (in->v.b.bi) && VREG (in->v.b.bi) != R_AX &&
			    VREG (in->v.b.bi) != R_DX) {
				sprintf (b, "%s", regname[VREG (in->v.b.bi)]);
			} else {
				t = scratch (pc, 0);
				if (t == R_AX || t == R_DX)
					t = R_BX;
				readop (in->v.b.bi, a, pc);
				printf ("\tmov %s, %s\n", regname[t], a);
				sprintf (b, "%s", regname[t]);
			}
			toreg (in->v.b.a, R_AX, pc);
			printf ("\txor dx, dx\n");
			printf ("\tdiv %s\n", b);
			stohome (in->v.b.dst,
				 in->v.b.bop == '/' ? R_AX : R_DX, pc);
			break;
		}
		if (in->v.b.bop == 'l' || in->v.b.bop == 'r') {
			toreg (in->v.b.a, R_AX, pc);
			if (in->v.b.fl & 8) {
				printf ("\tmov cl, %u\n", (unsigned) in->v.b.bi);
			} else {
				t = -1;
				for (r = R_AX; r <= R_DX; ++r)
					if (r != R_AX && !busy (pc, r)) {
						t = r;
						break;
					}
				if (t < 0)
					t = R_BX;
				avoid[t] = 1;
				toreg (in->v.b.bi, t, pc);
				printf ("\tmov cl, %s\n", bytename[t]);
			}
			printf ("\t%s ax, cl\n",
				in->v.b.bop == 'l' ? "shl" : "shr");
			stohome (in->v.b.dst, R_AX, pc);
			break;
		}
		if (in->v.b.fl & 8)
			sprintf (b, "%u", (unsigned) in->v.b.bi);
		else
			readop (in->v.b.bi, b, pc);
		readop (in->v.b.a, a, pc);
		if (VHOMED (in->v.b.dst)) {
			t = scratch (pc, 0);
			printf ("\tmov %s, %s\n", regname[t], a);
			printf ("\t%s %s, %s\n", bopname (in->v.b.bop),
				regname[t], b);
			printf ("\tmov [bp%+d], %s\n", vslot[in->v.b.dst],
				regname[t]);
		} else {
			toreg (in->v.b.a, VREG (in->v.b.dst), pc);
			printf ("\t%s %s, %s\n", bopname (in->v.b.bop),
				regname[VREG (in->v.b.dst)], b);
		}
		break;
	case 'c':
		{
			int fi = -1;
			for (i = 0; i < nins; ++i)
				if (ins[i].op == 'g' &&
				    ins[i].v.g.dst == in->v.c.a) {
					fi = i;
					break;
				}
			for (i = in->v.c.n - 1; i >= 0; --i) {
				readop (argpool[in->v.c.argi + i], a, pc);
				printf ("\tpush %s\n", a);
			}
			if (fi >= 0)
				printf ("\tcall %s\n", getsym (ins[fi].v.g.sym));
			else {
				readop (in->v.c.a, a, pc);
				printf ("\tcall %s\n", a);
			}
			if (in->v.c.n)
				printf ("\tadd sp, %d\n", 2 * in->v.c.n);
			stohome (in->v.c.dst, R_AX, pc);
		}
		break;
	case 'R':
		if (VHOMED (in->v.R.a))
			printf ("\tmov ax, [bp%+d]\n", vslot[in->v.R.a]);
		else if (VREG (in->v.R.a) != R_AX)
			printf ("\tmov ax, %s\n", regname[VREG (in->v.R.a)]);
		printf ("\tjmp %s.Lepilogue\n", getsym (funcsym));
		break;
	case 'r':
		printf ("\tjmp %s.Lepilogue\n", getsym (funcsym));
		break;
	case 'L':
		printf ("%s:\n", getsym (in->v.L.lab));
		break;
	case 'J':
		edge (LARGI (in->v.L.argn), LN (in->v.L.argn),
		    in->v.L.lab, pc);
		printf ("\tjmp %s\n", getsym (in->v.L.lab));
		break;
	case 'B':
		readop (in->v.B.a, a, pc);
		printf ("\ttest %s, %s\n", a, a);
		printf ("\tjnz .Ltrue%u\n", (unsigned) pc);
		edge (BARGI (in->v.B.argn) + BN (in->v.B.argn),
		    BNF (in->v.B.argn), in->v.B.lab2, pc);
		printf ("\tjmp %s\n", getsym (in->v.B.lab2));
		printf (".Ltrue%u:\n", (unsigned) pc);
		edge (BARGI (in->v.B.argn), BN (in->v.B.argn),
		    in->v.B.lab, pc);
		printf ("\tjmp %s\n", getsym (in->v.B.lab));
		break;
	}
}

static void
find_saves ()
{
	/* SI and DI are callee-saved and may be used for values or scratch,
	   so always preserve them (they sit at [bp-2] and [bp-4]). */
	saved[0] = R_SI;
	saved[1] = R_DI;
	nsaves = 2;
}

int
emit_func (sym, flags)
word sym;
byte flags;
{
	int i, k, src, sz;

	funcsym = sym;
	snprintf (funcname, 64, "%s", getsym (sym) + 1);
	compute_intervals ();
	allocate ();
	find_saves ();

	printf ("section .text\n");
	if ((flags & 3) == SC_GLOBAL)
		printf ("global %s\n", getsym (sym));
	printf ("%s:\n", getsym (sym));
	printf ("\tpush bp\n\tmov bp, sp\n");
	for (i = 0; i < nsaves; ++i)
		printf ("\tpush %s\n", regname[saved[i]]);
	if (framesize)
		printf ("\tsub sp, %d\n", framesize);

	/* copy incoming stack arguments into their local slots */
	src = 4;
	for (k = 0; k < nargs; ++k) {
		sz = locsize[k] < 2 ? 2 : (locsize[k] >= 4 ? 4 : 2);
		printf ("\tmov ax, [bp+%d]\n", src);
		printf ("\tmov [bp%+d], ax\n", locsite[k]);
		if (sz == 4) {
			printf ("\tmov ax, [bp+%d]\n", src + 2);
			printf ("\tmov [bp%+d], ax\n", locsite[k] - 2);
		}
		src += sz;
	}

	for (i = 0; i < nins; ++i)
		gen (&ins[i], i);

	printf ("%s.Lepilogue:\n", getsym (funcsym));
	/* saved[i] was pushed i-th, so it lives at [bp - 2*(i+1)] */
	for (i = nsaves - 1; i >= 0; --i)
		printf ("\tmov %s, [bp-%d]\n", regname[saved[i]],
			2 * (i + 1));
	printf ("\tmov sp, bp\n\tpop bp\n\tret\n");
	return 0;
}
