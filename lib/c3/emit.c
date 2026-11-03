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
use (v, lo, hi)
word v;
int lo, hi;
{
	touch (v, lo, hi);
	if (v < nval)
		vinfo[v] |= 0x80;
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
			if (ins[i].op == 'g')
				vinfo[ins[i].v.g.dst] |= 0x40;
			break;
		case 'c':
			touch (ins[i].v.c.dst, i, i);
			touch (ins[i].v.c.a, 0, i);
			for (k = 0; k < ins[i].v.c.n; ++k)
				use (AVAL (argpool[ins[i].v.c.argi + k]), 0, i);
			break;
		case 'C':
			touch (ins[i].v.C.dst, i, i);
			use (ins[i].v.C.a, 0, i);
			vinfo[ins[i].v.C.dst] = vinfo[ins[i].v.C.a] & 0x10;
			break;
		case '*':
			touch (ins[i].v.m.dst, i, i);
			use (ins[i].v.m.a, 0, i);
			vinfo[ins[i].v.m.a] |= 16;
			break;
		case '=':
			use (ins[i].v.m.dst, 0, i);
			use (ins[i].v.m.a, 0, i);
			vinfo[ins[i].v.m.dst] |= 16;
			break;
		case 'p':
		case 'q':
			touch (ins[i].v.g.dst, i, i);
			use (ins[i].v.g.sym, 0, i);
			break;
		case 'f':
			touch (ins[i].v.m.dst, i, i);
			use (ins[i].v.m.a, 0, i);
			vinfo[ins[i].v.m.a] |= 16;
			break;
		case 's':
			use (ins[i].v.m.dst, 0, i);
			use (ins[i].v.m.a, 0, i);
			vinfo[ins[i].v.m.dst] |= 16;
			break;
		case 'u':
			touch (ins[i].v.u.dst, i, i);
			use (ins[i].v.u.a, 0, i);
			break;
		case 'b':
			touch (ins[i].v.b.dst, i, i);
			use (ins[i].v.b.a, 0, i);
			if (!(ins[i].v.b.fl & 8))
				use (ins[i].v.b.bi, 0, i);
			break;
		case 'R':
			use (ins[i].v.R.a, 0, i);
			break;
		case 'J':
			for (k = 0; k < (int) LN (ins[i].v.L.argn); ++k)
				use (argpool[LARGI (ins[i].v.L.argn) + k], 0, i);
			break;
		case 'B':
			use (ins[i].v.B.a, 0, i);
			for (k = 0; k < (int) BN (ins[i].v.B.argn); ++k)
				use (argpool[BARGI (ins[i].v.B.argn) + k], 0, i);
			for (k = 0; k < (int) BNF (ins[i].v.B.argn); ++k)
				use (argpool[BARGI (ins[i].v.B.argn)
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
	int i, j, t, cand, victim, vend, norder, bestlo, src;
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

		/* A dword value needs two words; keep it in a frame slot
		   instead of trying to fit it into a single register. */
		if (VDW (v)) {
			vinfo[v] |= 8;
			continue;
		}

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
		vinfo[v] = (vinfo[v] & 0xf0) | cand;
		owner[cand] = v;
		if (nactive >= NREG)
			errx (1, "c3: allocator invariant broken");
		active[nactive++] = v;
	}

	/* frame: saved SI/DI occupy [bp-2],[bp-4]; slots sit below them.
	   A dword slot is stored little-endian: low word at vslot,
	   high word at vslot + 2. */
	framesize = 4;
	src = 4;			/* first incoming argument at [bp+4] */
	for (i = 0; i < nloc; ++i) {
		int sz = locsize[i] < 2 ? 2 : (locsize[i] >= 4 ? 4 : 2);
		if (i < nargs) {
			/* Parameters already live on the caller's stack: use
			   them in place instead of copying them into the frame. */
			locsite[i] = src;
			src += sz;
		} else {
			framesize += sz;
			locsite[i] = -framesize;
		}
	}
	j = 0;
	for (v = 0; v < nval; ++v)
		if (VHOMED (v)) {
			int sz = VDW (v) ? 4 : 2;
			framesize += sz;
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
memopoff (v, out, pc, off)
word v;
char *out;
int pc, off;
{
	int r;

	if (!VHOMED (v) && addrreg[VREG (v)]) {
		if (off)
			sprintf (out, "[%s+%d]", regname[VREG (v)], off);
		else
			sprintf (out, "[%s]", regname[VREG (v)]);
		return;
	}
	r = scratch (pc, 1);
	if (!VHOMED (v))
		printf ("\tmov %s, %s\n", regname[r], regname[VREG (v)]);
	else
		printf ("\tmov %s, [bp%+d]\n", regname[r], vslot[v]);
	sprintf (out, off ? "[%s+%d]" : "[%s]", regname[r], off);
}

static void
memop (v, out, pc)
word v;
char *out;
int pc;
{
	memopoff (v, out, pc, 0);
}

/* Put a near address into a register other than AX, which callers use
   as a scratch for the value being moved. */
static int
addrfor (v, pc)
word v;
int pc;
{
	int r;

	if (!VHOMED (v) && VREG (v) != R_AX && addrreg[VREG (v)])
		return VREG (v);
	avoid[R_AX] = 1;
	r = scratch (pc, 1);
	if (r == R_AX) {
		r = R_BX;
		avoid[r] = 1;
	}
	if (!VHOMED (v))
		printf ("\tmov %s, %s\n", regname[r], regname[VREG (v)]);
	else
		printf ("\tmov %s, [bp%+d]\n", regname[r], vslot[v]);
	return r;
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

/* dword values live in frame slots: low word at vslot, high at vslot+2 */
static void
loaddw (v, pc)
word v;
int pc;
{
	(void) pc;
	if (VHOMED (v)) {
		printf ("\tmov ax, [bp%+d]\n", vslot[v]);
		printf ("\tmov dx, [bp%+d]\n", vslot[v] + 2);
	}
}

static void
storedw (v, pc)
word v;
int pc;
{
	(void) pc;
	if (VHOMED (v)) {
		printf ("\tmov [bp%+d], ax\n", vslot[v]);
		printf ("\tmov [bp%+d], dx\n", vslot[v] + 2);
	}
}

/* an operand string for the low/high word of v */
static void
dwval (v, hi, out)
word v;
int hi;
char *out;
{
	if (!VDW (v) && hi) {
		sprintf (out, "0");
		return;
	}
	if (VHOMED (v))
		sprintf (out, "[bp%+d]", vslot[v] + (hi ? 2 : 0));
	else if (hi)
		sprintf (out, "0");
	else
		sprintf (out, "%s", regname[VREG (v)]);
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
		if (VDW (dst)) {
			if (VDW (src) && VHOMED (src)) {
				printf ("\tmov ax, [bp%+d]\n", vslot[src]);
				printf ("\tmov [bp%+d], ax\n", vslot[dst]);
				printf ("\tmov ax, [bp%+d]\n", vslot[src] + 2);
				printf ("\tmov [bp%+d], ax\n", vslot[dst] + 2);
			} else {
				readop (src, a, pc);
				printf ("\tmov [bp%+d], %s\n", vslot[dst], a);
				printf ("\tmov word [bp%+d], 0\n",
					vslot[dst] + 2);
			}
			continue;
		}
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
genfar (in, pc)
struct insn *in;
int pc;
{
	switch (in->op) {
	case 'f':
		printf ("\tpush es\n");
		printf ("\tmov bx, [bp%+d]\n", vslot[in->v.m.a]);
		printf ("\tmov es, [bp%+d]\n", vslot[in->v.m.a] + 2);
		if (in->v.m.size == SZ_B) {
			printf ("\tmov ax, 0\n");
			printf ("\tmov al, [es:bx]\n");
			stohome (in->v.m.dst, R_AX, pc);
		} else if (in->v.m.size == SZ_D) {
			printf ("\tmov ax, [es:bx]\n");
			if (VHOMED (in->v.m.dst))
				printf ("\tmov [bp%+d], ax\n",
					vslot[in->v.m.dst]);
			printf ("\tmov ax, [es:bx+2]\n");
			if (VHOMED (in->v.m.dst))
				printf ("\tmov [bp%+d], ax\n",
					vslot[in->v.m.dst] + 2);
		} else {
			printf ("\tmov ax, [es:bx]\n");
			stohome (in->v.m.dst, R_AX, pc);
		}
		printf ("\tpop es\n");
		break;
	case 's':
		printf ("\tpush es\n");
		printf ("\tmov bx, [bp%+d]\n", vslot[in->v.m.dst]);
		printf ("\tmov es, [bp%+d]\n", vslot[in->v.m.dst] + 2);
		if (in->v.m.size == SZ_D) {
			printf ("\tmov ax, [bp%+d]\n", vslot[in->v.m.a]);
			printf ("\tmov [es:bx], ax\n");
			printf ("\tmov ax, [bp%+d]\n",
				vslot[in->v.m.a] + 2);
			printf ("\tmov [es:bx+2], ax\n");
		} else if (in->v.m.size == SZ_B) {
			toreg (in->v.m.a, R_AX, pc);
			printf ("\tmov [es:bx], al\n");
		} else {
			if (VHOMED (in->v.m.a))
				printf ("\tmov ax, [bp%+d]\n",
					vslot[in->v.m.a]);
			else
				printf ("\tmov ax, %s\n",
					regname[VREG (in->v.m.a)]);
			printf ("\tmov [es:bx], ax\n");
		}
		printf ("\tpop es\n");
		break;
	}
}

static int
gendw (in, pc)
struct insn *in;
int pc;
{
			const char *lo, *hi;
			char al[32], bl[32], ah[32], bh[32];
			int cmp = cmpidx (in->v.b.bop);

			if ((in->v.b.fl & 3) != SZ_D)
				return 0;

			if (cmp >= 0) {
				int bt;
				char bsave[8];

				dwval (in->v.b.a, 0, al);
				dwval (in->v.b.bi, 0, bl);
				dwval (in->v.b.a, 1, ah);
				dwval (in->v.b.bi, 1, bh);
				if (!VHOMED (in->v.b.bi)
				    && VREG (in->v.b.bi) == R_AX) {
					bt = scratch (pc, 0);
					if (bt == R_AX)
						bt = R_BX;
					printf ("\tmov %s, ax\n", regname[bt]);
					sprintf (bsave, "%s", regname[bt]);
					sprintf (bl, "%s", bsave);
				}
				printf ("\tmov ax, %s\n", al);
				printf ("\tsub ax, %s\n", bl);
				printf ("\tmov bx, ax\n");
				printf ("\tmov ax, %s\n", ah);
				printf ("\tsbb ax, %s\n", bh);
				switch (in->v.b.bop) {
				case '<':
					printf ("\tjb .Ldwt%u\n", (unsigned) pc);
					break;
				case 'G':
					printf ("\tjae .Ldwt%u\n", (unsigned) pc);
					break;
				case '>':
					printf ("\tjb .Ldwf%u\n", (unsigned) pc);
					printf ("\tor ax, bx\n");
					printf ("\tjz .Ldwf%u\n", (unsigned) pc);
					printf ("\tjmp .Ldwt%u\n", (unsigned) pc);
					break;
				case 'L':
					printf ("\tjb .Ldwt%u\n", (unsigned) pc);
					printf ("\tor ax, bx\n");
					printf ("\tjz .Ldwt%u\n", (unsigned) pc);
					printf ("\tjmp .Ldwf%u\n", (unsigned) pc);
					break;
				case 'E':
					printf ("\tor ax, bx\n");
					printf ("\tjz .Ldwt%u\n", (unsigned) pc);
					break;
				case 'N':
					printf ("\tor ax, bx\n");
					printf ("\tjnz .Ldwt%u\n", (unsigned) pc);
					break;
				}
				printf (".Ldwf%u:\n", (unsigned) pc);
				printf ("\tmov word [bp%+d], 0\n",
					vslot[in->v.b.dst]);
				printf ("\tmov word [bp%+d], 0\n",
					vslot[in->v.b.dst] + 2);
				printf ("\tjmp .Ldwe%u\n", (unsigned) pc);
				printf (".Ldwt%u:\n", (unsigned) pc);
				printf ("\tmov word [bp%+d], 1\n",
					vslot[in->v.b.dst]);
				printf ("\tmov word [bp%+d], 0\n",
					vslot[in->v.b.dst] + 2);
				printf (".Ldwe%u:\n", (unsigned) pc);
				return 1;
			}

			if (in->v.b.bop == '+'
			    || in->v.b.bop == '-') {
				lo = in->v.b.bop == '+' ? "add" : "sub";
				hi = in->v.b.bop == '+' ? "adc" : "sbb";
			} else if (in->v.b.bop == '&'
				|| in->v.b.bop == '|'
				|| in->v.b.bop == '^') {
				lo = hi = in->v.b.bop == '&' ? "and"
				     : in->v.b.bop == '|' ? "or" : "xor";
			} else
				lo = hi = (const char *) 0;

			if (lo != (const char *) 0) {
				int adw = VDW (in->v.b.a);
				int bdw = VDW (in->v.b.bi);
				int bt;
				char bsave[8];
				const char *bop;

				if (VHOMED (in->v.b.bi))
					bop = (const char *) 0;
				else if (VREG (in->v.b.bi) != R_AX)
					bop = regname[VREG (in->v.b.bi)];
				else {
					bt = scratch (pc, 0);
					if (bt == R_AX)
						bt = R_BX;
					printf ("\tmov %s, ax\n", regname[bt]);
					sprintf (bsave, "%s", regname[bt]);
					bop = bsave;
				}

				if (VHOMED (in->v.b.a))
					printf ("\tmov ax, [bp%+d]\n",
						vslot[in->v.b.a]);
				else
					printf ("\tmov ax, %s\n",
						regname[VREG (in->v.b.a)]);
				if (bop != (const char *) 0)
					printf ("\t%s ax, %s\n", lo, bop);
				else
					printf ("\t%s ax, [bp%+d]\n", lo,
						vslot[in->v.b.bi]);
				printf ("\tmov [bp%+d], ax\n",
					vslot[in->v.b.dst]);

				if (adw && VHOMED (in->v.b.a))
					printf ("\tmov ax, [bp%+d]\n",
						vslot[in->v.b.a] + 2);
				else
					printf ("\tmov ax, 0\n");
				if (bdw && VHOMED (in->v.b.bi))
					printf ("\t%s ax, [bp%+d]\n", hi,
						vslot[in->v.b.bi] + 2);
				else
					printf ("\t%s ax, 0\n", hi);
				printf ("\tmov [bp%+d], ax\n",
					vslot[in->v.b.dst] + 2);
				return 1;
			}
		
	return 0;
}

static void
genb (in, pc)
struct insn *in;
int pc;
{
	char a[32], b[32];
	int r, t;

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
			return;
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
			return;
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
			return;
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
			return;
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
	return;
}

static void
gen (in, pc)
struct insn *in;
int pc;
{
	char a[32], m[32];
	int r, i, t;

	for (i = 0; i < NREG; ++i)
		avoid[i] = 0;

	switch (in->op) {
	case 'i':
		if (in->v.i.size == SZ_D) {
			if (VHOMED (in->v.i.dst)) {
				printf ("\tmov word [bp%+d], %u\n",
					vslot[in->v.i.dst],
					(unsigned) in->v.i.imm);
				printf ("\tmov word [bp%+d], %u\n",
					vslot[in->v.i.dst] + 2,
					(unsigned) in->v.i.immhi);
			}
		} else if (VHOMED (in->v.i.dst))
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
		if ((vinfo[in->v.g.dst] & 0xc0) == 0x40)
			break;
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
		if (VDW (in->v.C.dst)) {
			if (VDW (in->v.C.a)) {
				loaddw (in->v.C.a, pc);
			} else {
				toreg (in->v.C.a, R_AX, pc);
				if (in->v.C.fl == 2)
					printf ("\tcwd\n");
				else
					printf ("\txor dx, dx\n");
			}
			storedw (in->v.C.dst, pc);
		} else if (VDW (in->v.C.a)) {
			/* truncate to the low word */
			toreg (in->v.C.a, R_AX, pc);
			stohome (in->v.C.dst, R_AX, pc);
		} else {
			toreg (in->v.C.a, R_AX, pc);
			stohome (in->v.C.dst, R_AX, pc);
		}
		break;
	case '*':
		if (in->v.m.size == SZ_D) {
			int ar = addrfor (in->v.m.a, pc);
			printf ("\tmov ax, [%s]\n", regname[ar]);
			if (VHOMED (in->v.m.dst))
				printf ("\tmov [bp%+d], ax\n",
					vslot[in->v.m.dst]);
			printf ("\tmov ax, [%s+2]\n", regname[ar]);
			if (VHOMED (in->v.m.dst))
				printf ("\tmov [bp%+d], ax\n",
					vslot[in->v.m.dst] + 2);
			break;
		}
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
		if (in->v.m.size == SZ_D) {
			int ar = addrfor (in->v.m.dst, pc);
			if (VHOMED (in->v.m.a)) {
				printf ("\tmov ax, [bp%+d]\n",
					vslot[in->v.m.a]);
				printf ("\tmov [%s], ax\n", regname[ar]);
				printf ("\tmov ax, [bp%+d]\n",
					vslot[in->v.m.a] + 2);
				printf ("\tmov [%s+2], ax\n", regname[ar]);
			} else {
				printf ("\tmov ax, %s\n",
					regname[VREG (in->v.m.a)]);
				printf ("\tmov [%s], ax\n", regname[ar]);
				printf ("\tmov word [%s+2], 0\n", regname[ar]);
			}
			break;
		}
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
	case 'p':
	case 'q':
		if (VHOMED (in->v.g.dst)) {
			if (VHOMED (in->v.g.sym))
				printf ("\tmov ax, [bp%+d]\n",
					vslot[in->v.g.sym]);
			else
				printf ("\tmov ax, %s\n",
					regname[VREG (in->v.g.sym)]);
			printf ("\tmov [bp%+d], ax\n", vslot[in->v.g.dst]);
			printf ("\tmov [bp%+d], %s\n",
				vslot[in->v.g.dst] + 2,
				in->op == 'q' ? "ss" : "ds");
		}
		break;
	case 'f':
	case 's':
		genfar (in, pc);
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
		if (gendw (in, pc))
			break;
		genb (in, pc);
		break;
	case 'c':
		{
			int fi = -1;
			int bytes = 0;
			for (i = 0; i < nins; ++i)
				if (ins[i].op == 'g' &&
				    ins[i].v.g.dst == in->v.c.a) {
					fi = i;
					break;
				}
			for (i = 0; i < in->v.c.n; ++i)
				bytes += ADW (argpool[in->v.c.argi + i])
				      ? 4 : 2;
			for (i = in->v.c.n - 1; i >= 0; --i) {
				word av = AVAL (argpool[in->v.c.argi + i]);
				if (ADW (argpool[in->v.c.argi + i])
				    && VHOMED (av)) {
					printf ("\tmov ax, [bp%+d]\n",
						vslot[av] + 2);
					printf ("\tpush ax\n");
					printf ("\tmov ax, [bp%+d]\n",
						vslot[av]);
					printf ("\tpush ax\n");
				} else {
					readop (av, a, pc);
					printf ("\tpush %s\n", a);
				}
			}
			if (fi >= 0)
				printf ("\tcall %s\n", getsym (ins[fi].v.g.sym));
			else {
				readop (in->v.c.a, a, pc);
				printf ("\tcall %s\n", a);
			}
			if (in->v.c.n)
				printf ("\tadd sp, %d\n", bytes);
			if (VDW (in->v.c.dst))
				storedw (in->v.c.dst, pc);
			else
				stohome (in->v.c.dst, R_AX, pc);
		}
		break;
	case 'R':
		if (VDW (in->v.R.a))
			loaddw (in->v.R.a, pc);
		else if (VHOMED (in->v.R.a))
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
		if (VDW (in->v.B.a)) {
			printf ("\tmov ax, [bp%+d]\n",
				vslot[in->v.B.a]);
			printf ("\tor ax, [bp%+d]\n",
				vslot[in->v.B.a] + 2);
			printf ("\ttest ax, ax\n");
		} else {
			readop (in->v.B.a, a, pc);
			printf ("\ttest %s, %s\n", a, a);
		}
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
