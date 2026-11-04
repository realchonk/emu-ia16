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

static const int addrreg[NREG] = { 0, 1, 0, 0, 1, 1 };

static int	avoid[NREG];

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
			touch (ins[i].v.c.a, -1, i);
			for (k = 0; k < ins[i].v.c.n; ++k)
				use (AVAL (argpool[ins[i].v.c.argi + k]), -1, i);
			break;
		case 'C':
			touch (ins[i].v.C.dst, i, i);
			use (ins[i].v.C.a, -1, i);
			vinfo[ins[i].v.C.dst] = vinfo[ins[i].v.C.a] & 0x10;
			break;
		case '*':
			touch (ins[i].v.m.dst, i, i);
			use (ins[i].v.m.a, -1, i);
			vinfo[ins[i].v.m.a] |= 16;
			break;
		case '=':
			use (ins[i].v.m.dst, -1, i);
			use (ins[i].v.m.a, -1, i);
			vinfo[ins[i].v.m.dst] |= 16;
			break;
		case 'p':
		case 'q':
			touch (ins[i].v.g.dst, i, i);
			use (ins[i].v.g.sym, -1, i);
			break;
		case 'f':
			touch (ins[i].v.m.dst, i, i);
			use (ins[i].v.m.a, -1, i);
			vinfo[ins[i].v.m.a] |= 16;
			break;
		case 's':
			use (ins[i].v.m.dst, -1, i);
			use (ins[i].v.m.a, -1, i);
			vinfo[ins[i].v.m.dst] |= 16;
			break;
		case 'u':
			touch (ins[i].v.u.dst, i, i);
			use (ins[i].v.u.a, -1, i);
			break;
		case 'b':
			touch (ins[i].v.b.dst, i, i);
			use (ins[i].v.b.a, -1, i);
			if (!(ins[i].v.b.fl & 8))
				use (ins[i].v.b.bi, -1, i);
			break;
		case 'R':
			use (ins[i].v.R.a, -1, i);
			break;
		case 'J':
			for (k = 0; k < (int) LN (ins[i].v.L.argn); ++k)
				use (argpool[LARGI (ins[i].v.L.argn) + k], -1, i);
			break;
		case 'B':
			use (ins[i].v.B.a, -1, i);
			for (k = 0; k < (int) BN (ins[i].v.B.argn); ++k)
				use (argpool[BARGI (ins[i].v.B.argn) + k], -1, i);
			for (k = 0; k < (int) BNF (ins[i].v.B.argn); ++k)
				use (argpool[BARGI (ins[i].v.B.argn)
				    + BN (ins[i].v.B.argn) + k], -1, i);
			break;
		case 'L':
			for (k = 0; k < (int) LN (ins[i].v.L.argn); ++k)
				touch (argpool[LARGI (ins[i].v.L.argn) + k], i, i);
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
	/* AX, CX and DX are used as fixed scratch registers by the
	   code generator (shifts use CL, mul/div and casts use AX/DX), so
	   do not keep long-lived values there. */
	if (r == R_AX || r == R_CX || r == R_DX || r == R_DI)
		return 0;
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
		int sz = locsize[i] < 2 ? 2 : (int) locsize[i];
		if (sz & 1)
			++sz;
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

/* ---- MDIR operands and instructions ----------------------------------- */

struct opnd {
	byte	k;		/* 'R','B','M','I','S','T','G' */
	byte	r;
	word	v;
	word	s;
};

static void
o_reg (o, r)
struct opnd *o;
int r;
{
	o->k = 'R';
	o->r = r;
	o->v = 0;
	o->s = 0;
}

static void
o_seg (o, r)
struct opnd *o;
int r;
{
	o->k = 'G';
	o->r = r;
	o->v = 0;
	o->s = 0;
}

static void
o_imm (o, v)
struct opnd *o;
int v;
{
	o->k = 'I';
	o->r = 0;
	o->v = (word) v;
	o->s = 0;
}

static void
o_bp (o, v)
struct opnd *o;
int v;
{
	o->k = 'B';
	o->r = 0;
	o->v = (word) v;
	o->s = 0;
}

static void
o_mem (o, r, v)
struct opnd *o;
int r, v;
{
	o->k = 'M';
	o->r = r;
	o->v = (word) v;
	o->s = 0;
}

static void
o_sym (o, s, a)
struct opnd *o;
word s;
int a;
{
	o->k = 'S';
	o->r = 0;
	o->s = s;
	o->v = (word) a;
}

static void
o_str (o, a)
struct opnd *o;
int a;
{
	o->k = 'T';
	o->r = 0;
	o->s = 0;
	o->v = (word) a;
}

static void
eop (o)
struct opnd *o;
{
	md_b (o->k);
	switch (o->k) {
	case 'R':
	case 'G':
		md_b (o->r);
		break;
	case 'B':
	case 'I':
	case 'T':
		md_w (o->v);
		break;
	case 'M':
		md_b (o->r);
		md_w (o->v);
		break;
	case 'S':
		md_w (o->s);
		md_w (o->v);
		break;
	}
}

static void
i2 (tag, sz, a, b)
int tag, sz;
struct opnd *a, *b;
{
	md_b (tag);
	md_b (sz);
	eop (a);
	eop (b);
}

static void
i1 (tag, a)
int tag;
struct opnd *a;
{
	md_b (tag);
	eop (a);
}

static void
mov_w (d, s)
struct opnd *d, *s;
{
	i2 ('m', SZ_W, d, s);
}

static void
mov_b (d, s)
struct opnd *d, *s;
{
	i2 ('m', SZ_B, d, s);
}

static void
alu (op, sz, d, s)
int op, sz;
struct opnd *d, *s;
{
	md_b ('a');
	md_b (op);
	md_b (sz);
	eop (d);
	eop (s);
}

static void
test_w (a, b)
struct opnd *a, *b;
{
	i2 ('t', SZ_W, a, b);
}

static void
lea (d, s)
struct opnd *d, *s;
{
	md_b ('l');
	eop (d);
	eop (s);
}

static void
load (sz, d, a)
int sz;
struct opnd *d, *a;
{
	md_b ('d');
	md_b (sz);
	eop (d);
	eop (a);
}

static void
store (sz, a, s)
int sz;
struct opnd *a, *s;
{
	md_b ('s');
	md_b (sz);
	eop (a);
	eop (s);
}

static void
push (a)
struct opnd *a;
{
	i1 ('p', a);
}

static void
pop (a)
struct opnd *a;
{
	i1 ('o', a);
}

static void
call (a)
struct opnd *a;
{
	i1 ('c', a);
}

static void
jump (lab)
word lab;
{
	struct opnd o;

	o_sym (&o, lab, 0);
	i1 ('j', &o);
}

static void
jcc (cc, lab)
int cc;
word lab;
{
	struct opnd o;

	md_b ('J');
	md_b (cc);
	o_sym (&o, lab, 0);
	eop (&o);
}

static void
ret ()
{
	md_b ('r');
}

static void
fixed (op)
int op;
{
	md_b ('x');
	md_b (op);
}

static void
label (lab)
word lab;
{
	md_b ('L');
	md_w (lab);
}

static void
unary (op, sz, a)
int op, sz;
struct opnd *a;
{
	md_b ('u');
	md_b (op);
	md_b (sz);
	eop (a);
}

static void
shift (op, sz, a, b)
int op, sz;
struct opnd *a, *b;
{
	md_b ('h');
	md_b (op);
	md_b (sz);
	eop (a);
	eop (b);
}

/* Synthetic labels live in the generated-label namespace.  c3 reserves a
   block of four ids per instruction, above every label the SSA already
   used. */
static int	intbase;
static word	epilbl;

static word
ilab (pc, k)
int pc, k;
{
	return 0x8000 | ((intbase + pc * 4 + k) & 0x7fff);
}

/* ---- operand construction --------------------------------------------- */

static void
readop (v, o, pc)
word v;
struct opnd *o;
int pc;
{
	int r;
	struct opnd a;

	if (!VHOMED (v)) {
		o_reg (o, VREG (v));
		return;
	}
	r = scratch (pc, VADDR (v) != 0);
	o_bp (&a, vslot[v]);
	o_reg (o, r);
	mov_w (o, &a);
}

static void
memopoff (v, o, pc, off)
word v;
struct opnd *o;
int pc, off;
{
	int r;
	struct opnd a, b;

	if (!VHOMED (v) && addrreg[VREG (v)]) {
		o_mem (o, VREG (v), off);
		return;
	}
	r = scratch (pc, 1);
	if (!VHOMED (v)) {
		o_reg (&a, r);
		o_reg (&b, VREG (v));
		mov_w (&a, &b);
	} else {
		o_reg (&a, r);
		o_bp (&b, vslot[v]);
		mov_w (&a, &b);
	}
	o_mem (o, r, off);
}

static void
memop (v, o, pc)
word v;
struct opnd *o;
int pc;
{
	memopoff (v, o, pc, 0);
}

/* Put a near address into a register other than AX, which callers use
   as a scratch for the value being moved. */
static int
addrfor (v, pc)
word v;
int pc;
{
	int r;
	struct opnd a, b;

	if (!VHOMED (v) && VREG (v) != R_AX && addrreg[VREG (v)])
		return VREG (v);
	avoid[R_AX] = 1;
	r = scratch (pc, 1);
	if (r == R_AX) {
		r = R_BX;
		avoid[r] = 1;
	}
	o_reg (&a, r);
	if (!VHOMED (v))
		o_reg (&b, VREG (v));
	else
		o_bp (&b, vslot[v]);
	mov_w (&a, &b);
	return r;
}

static void
toreg (v, r, pc)
word v;
int r;
int pc;
{
	struct opnd a, b;

	(void) pc;
	if (!VHOMED (v)) {
		if (VREG (v) != r) {
			o_reg (&a, r);
			o_reg (&b, VREG (v));
			mov_w (&a, &b);
		}
	} else {
		o_reg (&a, r);
		o_bp (&b, vslot[v]);
		mov_w (&a, &b);
	}
}

static void
stohome (v, r, pc)
word v;
int r;
int pc;
{
	struct opnd a, b;

	(void) pc;
	if (VHOMED (v)) {
		o_bp (&a, vslot[v]);
		o_reg (&b, r);
		mov_w (&a, &b);
	} else if (VREG (v) != r) {
		o_reg (&a, VREG (v));
		o_reg (&b, r);
		mov_w (&a, &b);
	}
}

/* dword values live in frame slots: low word at vslot, high at vslot+2 */
static void
loaddw (v, pc)
word v;
int pc;
{
	struct opnd a, b;

	(void) pc;
	if (VHOMED (v)) {
		o_reg (&a, R_AX);
		o_bp (&b, vslot[v]);
		mov_w (&a, &b);
		o_reg (&a, R_DX);
		o_bp (&b, vslot[v] + 2);
		mov_w (&a, &b);
	}
}

static void
storedw (v, pc)
word v;
int pc;
{
	struct opnd a, b;

	(void) pc;
	if (VHOMED (v)) {
		o_bp (&a, vslot[v]);
		o_reg (&b, R_AX);
		mov_w (&a, &b);
		o_bp (&a, vslot[v] + 2);
		o_reg (&b, R_DX);
		mov_w (&a, &b);
	}
}

/* an operand for the low/high word of v */
static void
dwval (v, hi, o)
word v;
int hi;
struct opnd *o;
{
	if (!VDW (v) && hi) {
		o_imm (o, 0);
		return;
	}
	if (VHOMED (v)) {
		o_bp (o, vslot[v] + (hi ? 2 : 0));
		return;
	}
	if (hi)
		o_imm (o, 0);
	else
		o_reg (o, VREG (v));
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
	struct opnd a, b;

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
				o_reg (&a, R_AX);
				o_bp (&b, vslot[src]);
				mov_w (&a, &b);
				o_bp (&a, vslot[dst]);
				mov_w (&a, &b);
				o_reg (&a, R_AX);
				o_bp (&b, vslot[src] + 2);
				mov_w (&a, &b);
				o_bp (&a, vslot[dst] + 2);
				mov_w (&a, &b);
			} else {
				readop (src, &a, pc);
				o_bp (&b, vslot[dst]);
				mov_w (&b, &a);
				o_imm (&a, 0);
				o_bp (&b, vslot[dst] + 2);
				mov_w (&b, &a);
			}
			continue;
		}
		readop (src, &a, pc);
		if (VHOMED (dst)) {
			o_bp (&b, vslot[dst]);
			mov_w (&b, &a);
		} else if (!(a.k == 'R' && a.r == VREG (dst))) {
			o_reg (&b, VREG (dst));
			mov_w (&b, &a);
		}
	}
}

static int
aluop (op)
int op;
{
	switch (op) {
	case '+': return 'a';
	case '-': return 's';
	case '&': return 'n';
	case '|': return 'o';
	case '^': return 'x';
	}
	return '?';
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

static const int cmpcc[] = { 2, 6, 7, 3, 4, 5 };
static const int cmpccs[] = { 12, 14, 15, 13, 4, 5 };

static void
genfar (in, pc)
struct insn *in;
int pc;
{
	struct opnd a, b, m;

	switch (in->op) {
	case 'f':
		o_seg (&a, SEG_ES);
		push (&a);
		o_reg (&a, R_BX);
		o_bp (&b, vslot[in->v.m.a]);
		mov_w (&a, &b);
		o_seg (&a, SEG_ES);
		o_bp (&b, vslot[in->v.m.a] + 2);
		mov_w (&a, &b);
		o_mem (&m, R_BX, 0);
		if (in->v.m.size == SZ_B) {
			o_reg (&a, R_AX);
			o_imm (&b, 0);
			mov_w (&a, &b);
			fixed (0x26);			/* es: */
			o_reg (&a, R_AX);		/* al */
			load (SZ_B, &a, &m);
			stohome (in->v.m.dst, R_AX, pc);
		} else if (in->v.m.size == SZ_D) {
			fixed (0x26);
			o_reg (&a, R_AX);
			load (SZ_W, &a, &m);
			if (VHOMED (in->v.m.dst)) {
				o_bp (&b, vslot[in->v.m.dst]);
				mov_w (&b, &a);
			}
			fixed (0x26);
			o_mem (&m, R_BX, 2);
			o_reg (&a, R_AX);
			load (SZ_W, &a, &m);
			if (VHOMED (in->v.m.dst)) {
				o_bp (&b, vslot[in->v.m.dst] + 2);
				mov_w (&b, &a);
			}
		} else {
			fixed (0x26);
			o_reg (&a, R_AX);
			load (SZ_W, &a, &m);
			stohome (in->v.m.dst, R_AX, pc);
		}
		o_seg (&a, SEG_ES);
		pop (&a);
		break;
	case 's':
		o_seg (&a, SEG_ES);
		push (&a);
		o_reg (&a, R_BX);
		o_bp (&b, vslot[in->v.m.dst]);
		mov_w (&a, &b);
		o_seg (&a, SEG_ES);
		o_bp (&b, vslot[in->v.m.dst] + 2);
		mov_w (&a, &b);
		o_mem (&m, R_BX, 0);
		if (in->v.m.size == SZ_D) {
			o_reg (&a, R_AX);
			o_bp (&b, vslot[in->v.m.a]);
			mov_w (&a, &b);
			fixed (0x26);
			store (SZ_W, &m, &a);
			o_reg (&a, R_AX);
			o_bp (&b, vslot[in->v.m.a] + 2);
			mov_w (&a, &b);
			o_mem (&m, R_BX, 2);
			fixed (0x26);
			store (SZ_W, &m, &a);
		} else if (in->v.m.size == SZ_B) {
			toreg (in->v.m.a, R_AX, pc);
			fixed (0x26);
			o_reg (&a, R_AX);		/* al */
			store (SZ_B, &m, &a);
		} else {
			if (VHOMED (in->v.m.a)) {
				o_reg (&a, R_AX);
				o_bp (&b, vslot[in->v.m.a]);
				mov_w (&a, &b);
			} else {
				o_reg (&a, R_AX);
				o_reg (&b, VREG (in->v.m.a));
				mov_w (&a, &b);
			}
			fixed (0x26);
			store (SZ_W, &m, &a);
		}
		o_seg (&a, SEG_ES);
		pop (&a);
		break;
	}
}

static int
gendw (in, pc)
struct insn *in;
int pc;
{
	struct opnd al, bl, ah, bh, a, b;
	int cmp = cmpidx (in->v.b.bop);

	if ((in->v.b.fl & 3) != SZ_D)
		return 0;

	if (cmp >= 0) {
		int bt;
		word dwt = ilab (pc, 0), dwf = ilab (pc, 1), dwe = ilab (pc, 2);

		dwval (in->v.b.a, 0, &al);
		dwval (in->v.b.bi, 0, &bl);
		dwval (in->v.b.a, 1, &ah);
		dwval (in->v.b.bi, 1, &bh);
		if (!VHOMED (in->v.b.bi)
		    && VREG (in->v.b.bi) == R_AX) {
			bt = R_DI;
			o_reg (&a, bt);
			o_reg (&b, R_AX);
			mov_w (&a, &b);
			o_reg (&bl, bt);
		}
		o_reg (&a, R_AX);
		mov_w (&a, &al);
		o_reg (&a, R_AX);
		alu ('s', SZ_W, &a, &bl);
		o_reg (&a, R_DI);
		o_reg (&b, R_AX);
		mov_w (&a, &b);
		o_reg (&a, R_AX);
		mov_w (&a, &ah);
		o_reg (&a, R_AX);
		alu ('b', SZ_W, &a, &bh);
		switch (in->v.b.bop) {
		case '<':
			jcc (2, dwt);
			break;
		case 'G':
			jcc (3, dwt);
			break;
		case '>':
			jcc (2, dwf);
			o_reg (&a, R_AX);
			o_reg (&b, R_DI);
			alu ('o', SZ_W, &a, &b);
			jcc (4, dwf);
			jump (dwt);
			break;
		case 'L':
			jcc (2, dwt);
			o_reg (&a, R_AX);
			o_reg (&b, R_DI);
			alu ('o', SZ_W, &a, &b);
			jcc (4, dwt);
			jump (dwf);
			break;
		case 'E':
			o_reg (&a, R_AX);
			o_reg (&b, R_DI);
			alu ('o', SZ_W, &a, &b);
			jcc (4, dwt);
			break;
		case 'N':
			o_reg (&a, R_AX);
			o_reg (&b, R_DI);
			alu ('o', SZ_W, &a, &b);
			jcc (5, dwt);
			break;
		}
		label (dwf);
		o_bp (&a, vslot[in->v.b.dst]);
		o_imm (&b, 0);
		mov_w (&a, &b);
		o_bp (&a, vslot[in->v.b.dst] + 2);
		mov_w (&a, &b);
		jump (dwe);
		label (dwt);
		o_bp (&a, vslot[in->v.b.dst]);
		o_imm (&b, 1);
		mov_w (&a, &b);
		o_imm (&b, 0);
		o_bp (&a, vslot[in->v.b.dst] + 2);
		mov_w (&a, &b);
		label (dwe);
		return 1;
	}

	if (in->v.b.bop == '+' || in->v.b.bop == '-') {
		int adw = VDW (in->v.b.a);
		int bdw = VDW (in->v.b.bi);
		int bt;
		int lo = in->v.b.bop == '+' ? 'a' : 's';
		int hi = in->v.b.bop == '+' ? 'c' : 'b';

		if (VHOMED (in->v.b.bi)) {
			o_bp (&b, vslot[in->v.b.bi]);
		} else if (VREG (in->v.b.bi) != R_AX) {
			o_reg (&b, VREG (in->v.b.bi));
		} else {
			bt = R_DI;
			o_reg (&a, bt);
			o_reg (&b, R_AX);
			mov_w (&a, &b);
			o_reg (&b, bt);
		}

		if (VHOMED (in->v.b.a)) {
			o_reg (&a, R_AX);
			o_bp (&al, vslot[in->v.b.a]);
			mov_w (&a, &al);
		} else {
			o_reg (&a, R_AX);
			o_reg (&al, VREG (in->v.b.a));
			mov_w (&a, &al);
		}
		o_reg (&a, R_AX);
		alu (lo, SZ_W, &a, &b);
		o_bp (&a, vslot[in->v.b.dst]);
		o_reg (&al, R_AX);
		mov_w (&a, &al);

		if (adw && VHOMED (in->v.b.a)) {
			o_reg (&a, R_AX);
			o_bp (&al, vslot[in->v.b.a] + 2);
			mov_w (&a, &al);
		} else {
			o_reg (&a, R_AX);
			o_imm (&al, 0);
			mov_w (&a, &al);
		}
		if (bdw && VHOMED (in->v.b.bi)) {
			o_reg (&a, R_AX);
			o_bp (&al, vslot[in->v.b.bi] + 2);
			alu (hi, SZ_W, &a, &al);
		} else {
			o_reg (&a, R_AX);
			o_imm (&al, 0);
			alu (hi, SZ_W, &a, &al);
		}
		o_bp (&a, vslot[in->v.b.dst] + 2);
		o_reg (&al, R_AX);
		mov_w (&a, &al);
		return 1;
	}

	if (in->v.b.bop == '&' || in->v.b.bop == '|'
	    || in->v.b.bop == '^') {
		int adw = VDW (in->v.b.a);
		int bdw = VDW (in->v.b.bi);
		int bt;
		int op = in->v.b.bop == '&' ? 'n'
		       : in->v.b.bop == '|' ? 'o' : 'x';

		if (VHOMED (in->v.b.bi)) {
			o_bp (&b, vslot[in->v.b.bi]);
		} else if (VREG (in->v.b.bi) != R_AX) {
			o_reg (&b, VREG (in->v.b.bi));
		} else {
			bt = R_DI;
			o_reg (&a, bt);
			o_reg (&b, R_AX);
			mov_w (&a, &b);
			o_reg (&b, bt);
		}
		if (VHOMED (in->v.b.a)) {
			o_reg (&a, R_AX);
			o_bp (&al, vslot[in->v.b.a]);
			mov_w (&a, &al);
		} else {
			o_reg (&a, R_AX);
			o_reg (&al, VREG (in->v.b.a));
			mov_w (&a, &al);
		}
		o_reg (&a, R_AX);
		alu (op, SZ_W, &a, &b);
		o_bp (&a, vslot[in->v.b.dst]);
		o_reg (&al, R_AX);
		mov_w (&a, &al);

		if (adw && VHOMED (in->v.b.a)) {
			o_reg (&a, R_AX);
			o_bp (&al, vslot[in->v.b.a] + 2);
			mov_w (&a, &al);
		} else {
			o_reg (&a, R_AX);
			o_imm (&al, 0);
			mov_w (&a, &al);
		}
		if (bdw && VHOMED (in->v.b.bi)) {
			o_reg (&a, R_AX);
			o_bp (&al, vslot[in->v.b.bi] + 2);
			alu (op, SZ_W, &a, &al);
		} else {
			o_reg (&a, R_AX);
			o_imm (&al, 0);
			alu (op, SZ_W, &a, &al);
		}
		o_bp (&a, vslot[in->v.b.dst] + 2);
		o_reg (&al, R_AX);
		mov_w (&a, &al);
		return 1;
	}

	return 0;
}

static void
genb (in, pc)
struct insn *in;
int pc;
{
	struct opnd a, b, x;
	int r, t;

	r = cmpidx (in->v.b.bop);
	if (r >= 0) {
		word setl = ilab (pc, 0), endl = ilab (pc, 1);

		readop (in->v.b.a, &a, pc);
		if (in->v.b.fl & 8)
			o_imm (&b, in->v.b.bi);
		else
			readop (in->v.b.bi, &b, pc);
		alu ('m', SZ_W, &a, &b);
		jcc (((in->v.b.fl & 4) ? cmpccs : cmpcc)[r], setl);
		if (VHOMED (in->v.b.dst)) {
			o_bp (&a, vslot[in->v.b.dst]);
			o_imm (&b, 0);
			mov_w (&a, &b);
		} else {
			o_reg (&a, VREG (in->v.b.dst));
			o_imm (&b, 0);
			mov_w (&a, &b);
		}
		jump (endl);
		label (setl);
		if (VHOMED (in->v.b.dst)) {
			o_bp (&a, vslot[in->v.b.dst]);
			o_imm (&b, 1);
			mov_w (&a, &b);
		} else {
			o_reg (&a, VREG (in->v.b.dst));
			o_imm (&b, 1);
			mov_w (&a, &b);
		}
		label (endl);
		return;
	}
	if (in->v.b.bop == '*') {
		if (!VHOMED (in->v.b.bi) && VREG (in->v.b.bi) != R_AX &&
		    VREG (in->v.b.bi) != R_DX) {
			o_reg (&b, VREG (in->v.b.bi));
		} else {
			t = scratch (pc, 0);
			if (t == R_AX || t == R_DX)
				t = R_CX;
			readop (in->v.b.bi, &a, pc);
			o_reg (&b, t);
			mov_w (&b, &a);
		}
		toreg (in->v.b.a, R_AX, pc);
		unary ('*', SZ_W, &b);
		stohome (in->v.b.dst, R_AX, pc);
		return;
	}
	if (in->v.b.bop == '/' || in->v.b.bop == '%') {
		if (!VHOMED (in->v.b.bi) && VREG (in->v.b.bi) != R_AX &&
		    VREG (in->v.b.bi) != R_DX) {
			o_reg (&b, VREG (in->v.b.bi));
		} else {
			t = scratch (pc, 0);
			if (t == R_AX || t == R_DX)
				t = R_CX;
			readop (in->v.b.bi, &a, pc);
			o_reg (&b, t);
			mov_w (&b, &a);
		}
		toreg (in->v.b.a, R_AX, pc);
		o_reg (&x, R_DX);
		o_reg (&a, R_DX);
		alu ('x', SZ_W, &x, &a);
		unary ('/', SZ_W, &b);
		stohome (in->v.b.dst,
			 in->v.b.bop == '/' ? R_AX : R_DX, pc);
		return;
	}
	if (in->v.b.bop == 'l' || in->v.b.bop == 'r') {
		o_reg (&x, R_CX);		/* cl */
		toreg (in->v.b.a, R_AX, pc);
		if (in->v.b.fl & 8) {
			o_imm (&a, in->v.b.bi);
			mov_b (&x, &a);
		} else {
			t = -1;
			for (r = R_AX; r <= R_DX; ++r)
				if (r != R_AX && !busy (pc, r)) {
					t = r;
					break;
				}
			if (t < 0)
				t = R_CX;
			avoid[t] = 1;
			toreg (in->v.b.bi, t, pc);
			o_reg (&a, t);
			mov_b (&x, &a);
		}
		o_reg (&a, R_AX);
		shift (in->v.b.bop == 'l' ? 'l' : 'r', SZ_W, &a, &x);
		stohome (in->v.b.dst, R_AX, pc);
		return;
	}
	if (in->v.b.fl & 8)
		o_imm (&b, in->v.b.bi);
	else
		readop (in->v.b.bi, &b, pc);
	readop (in->v.b.a, &a, pc);
	if (VHOMED (in->v.b.dst)) {
		t = scratch (pc, 0);
		o_reg (&x, t);
		mov_w (&x, &a);
		alu (aluop (in->v.b.bop), SZ_W, &x, &b);
		o_bp (&a, vslot[in->v.b.dst]);
		mov_w (&a, &x);
	} else {
		int dr = VREG (in->v.b.dst);

		/* two-address: moving a into dst would clobber b if b
		   already lives in dst.  AX is reserved as scratch. */
		if (b.k == 'R' && b.r == dr && a.r != dr) {
			o_reg (&x, R_AX);
			mov_w (&x, &b);
			b = x;
		}
		toreg (in->v.b.a, dr, pc);
		o_reg (&a, dr);
		alu (aluop (in->v.b.bop), SZ_W, &a, &b);
	}
}

static void
genlit (in, pc)
struct insn *in;
int pc;
{
	struct opnd a, b;
	int t;

	switch (in->op) {
	case 'i':
		if (in->v.i.size == SZ_D) {
			if (VHOMED (in->v.i.dst)) {
				o_bp (&a, vslot[in->v.i.dst]);
				o_imm (&b, in->v.i.imm);
				mov_w (&a, &b);
				o_bp (&a, vslot[in->v.i.dst] + 2);
				o_imm (&b, in->v.i.immhi);
				mov_w (&a, &b);
			}
		} else if (VHOMED (in->v.i.dst)) {
			o_bp (&a, vslot[in->v.i.dst]);
			o_imm (&b, in->v.i.imm);
			mov_w (&a, &b);
		} else {
			o_reg (&a, VREG (in->v.i.dst));
			o_imm (&b, in->v.i.imm);
			mov_w (&a, &b);
		}
		break;
	case 'S':
		o_str (&b, in->v.S.six);
		if (VHOMED (in->v.S.dst)) {
			o_bp (&a, vslot[in->v.S.dst]);
			mov_w (&a, &b);
		} else {
			o_reg (&a, VREG (in->v.S.dst));
			mov_w (&a, &b);
		}
		break;
	case 'g':
		if ((vinfo[in->v.g.dst] & 0xc0) == 0x40)
			break;
		o_sym (&b, in->v.g.sym, 0);
		if (VHOMED (in->v.g.dst)) {
			o_bp (&a, vslot[in->v.g.dst]);
			mov_w (&a, &b);
		} else {
			o_reg (&a, VREG (in->v.g.dst));
			mov_w (&a, &b);
		}
		break;
	case 'l':
		o_bp (&b, locsite[in->v.l.slot]);
		if (!VHOMED (in->v.l.dst)) {
			o_reg (&a, VREG (in->v.l.dst));
			lea (&a, &b);
		} else {
			t = scratch (pc, 1);
			o_reg (&a, t);
			lea (&a, &b);
			o_bp (&b, vslot[in->v.l.dst]);
			mov_w (&b, &a);
		}
		break;
	case 'C':
		if (VDW (in->v.C.dst)) {
			if (VDW (in->v.C.a)) {
				loaddw (in->v.C.a, pc);
			} else {
				toreg (in->v.C.a, R_AX, pc);
				if (in->v.C.fl == 2)
					fixed (0x99);		/* cwd */
				else {
					o_reg (&a, R_DX);
					o_reg (&b, R_DX);
					alu ('x', SZ_W, &a, &b);
				}
			}
			storedw (in->v.C.dst, pc);
		} else if (VDW (in->v.C.a)) {
			toreg (in->v.C.a, R_AX, pc);
			stohome (in->v.C.dst, R_AX, pc);
		} else {
			toreg (in->v.C.a, R_AX, pc);
			stohome (in->v.C.dst, R_AX, pc);
		}
		break;
	case 'p':
	case 'q':
		if (VHOMED (in->v.g.dst)) {
			if (VHOMED (in->v.g.sym)) {
				o_reg (&a, R_AX);
				o_bp (&b, vslot[in->v.g.sym]);
				mov_w (&a, &b);
			} else {
				o_reg (&a, R_AX);
				o_reg (&b, VREG (in->v.g.sym));
				mov_w (&a, &b);
			}
			o_bp (&b, vslot[in->v.g.dst]);
			o_reg (&a, R_AX);
			mov_w (&b, &a);
			o_imm (&a, in->op == 'q' ? SEG_SS : SEG_DS);
			o_bp (&b, vslot[in->v.g.dst] + 2);
			mov_w (&b, &a);
		}
		break;
	}
}

static void
genst (in, pc)
struct insn *in;
int pc;
{
	struct opnd a, b, m, x;
	int r, t;

	switch (in->op) {
	case '*':
		if (in->v.m.size == SZ_D) {
			int ar = addrfor (in->v.m.a, pc);

			o_reg (&a, R_AX);
			o_mem (&m, ar, 0);
			load (SZ_W, &a, &m);
			if (VHOMED (in->v.m.dst)) {
				o_bp (&m, vslot[in->v.m.dst]);
				mov_w (&m, &a);
			}
			o_reg (&a, R_AX);
			o_mem (&m, ar, 2);
			load (SZ_W, &a, &m);
			if (VHOMED (in->v.m.dst)) {
				o_bp (&m, vslot[in->v.m.dst] + 2);
				mov_w (&m, &a);
			}
			break;
		}
		memop (in->v.m.a, &m, pc);
		if (in->v.m.size == SZ_B) {
			r = !VHOMED (in->v.m.dst) ? VREG (in->v.m.dst)
						    : scratch (pc, 0);
			if (r == R_SI || r == R_DI) {
				o_reg (&a, R_AX);
				o_imm (&b, 0);
				mov_w (&a, &b);
				o_reg (&a, R_AX);
				load (SZ_B, &a, &m);
				stohome (in->v.m.dst, R_AX, pc);
			} else {
				o_reg (&a, r);
				o_imm (&b, 0);
				mov_w (&a, &b);
				o_reg (&a, r);
				load (SZ_B, &a, &m);
				if (VHOMED (in->v.m.dst)) {
					o_bp (&b, vslot[in->v.m.dst]);
					o_reg (&a, r);
					mov_w (&b, &a);
				}
			}
		} else {
			r = !VHOMED (in->v.m.dst) ? VREG (in->v.m.dst)
						    : scratch (pc, 0);
			o_reg (&a, r);
			load (SZ_W, &a, &m);
			if (VHOMED (in->v.m.dst)) {
				o_bp (&b, vslot[in->v.m.dst]);
				mov_w (&b, &a);
			}
		}
		break;
	case '=':
		if (in->v.m.size == SZ_D) {
			int ar = addrfor (in->v.m.dst, pc);

			if (VHOMED (in->v.m.a)) {
				o_reg (&a, R_AX);
				o_bp (&m, vslot[in->v.m.a]);
				mov_w (&a, &m);
				o_mem (&m, ar, 0);
				store (SZ_W, &m, &a);
				o_reg (&a, R_AX);
				o_bp (&m, vslot[in->v.m.a] + 2);
				mov_w (&a, &m);
				o_mem (&m, ar, 2);
				store (SZ_W, &m, &a);
			} else {
				o_reg (&a, VREG (in->v.m.a));
				o_mem (&m, ar, 0);
				store (SZ_W, &m, &a);
				o_imm (&a, 0);
				o_mem (&m, ar, 2);
				store (SZ_W, &m, &a);
			}
			break;
		}
		if (in->v.m.size == SZ_B) {
			t = -1;
			for (r = R_AX; r <= R_DX; ++r)
				if (!busy (pc, r)) {
					t = r;
					break;
				}
			if (t < 0)
				t = R_CX;
			avoid[t] = 1;
			toreg (in->v.m.a, t, pc);
			memop (in->v.m.dst, &m, pc);
			o_reg (&x, t);
			store (SZ_B, &m, &x);
		} else {
			readop (in->v.m.a, &a, pc);
			memop (in->v.m.dst, &m, pc);
			store (SZ_W, &m, &a);
		}
		break;
	}
}

static void
genarith (in, pc)
struct insn *in;
int pc;
{
	struct opnd a, b;

	switch (in->op) {
	case 'u':
		toreg (in->v.u.a, R_AX, pc);
		if (in->v.u.bop == '-') {
			o_reg (&a, R_AX);
			unary ('-', SZ_W, &a);
		} else if (in->v.u.bop == '~') {
			o_reg (&a, R_AX);
			unary ('~', SZ_W, &a);
		} else {
			word l = ilab (pc, 0);

			o_reg (&a, R_AX);
			test_w (&a, &a);
			o_reg (&a, R_AX);
			o_imm (&b, 0);
			mov_w (&a, &b);
			jcc (5, l);
			o_reg (&a, R_AX);
			o_imm (&b, 1);
			mov_w (&a, &b);
			label (l);
		}
		stohome (in->v.u.dst, R_AX, pc);
		break;
	case 'b':
		if (gendw (in, pc))
			break;
		genb (in, pc);
		break;
	}
}

static void
gencall (in, pc)
struct insn *in;
int pc;
{
	struct opnd a, b;
	int fi = -1;
	int bytes = 0;
	int i;

	for (i = 0; i < nins; ++i)
		if (ins[i].op == 'g' && ins[i].v.g.dst == in->v.c.a) {
			fi = i;
			break;
		}
	for (i = 0; i < in->v.c.n; ++i)
		bytes += ADW (argpool[in->v.c.argi + i]) ? 4 : 2;
	for (i = in->v.c.n - 1; i >= 0; --i) {
		word av = AVAL (argpool[in->v.c.argi + i]);
		if (ADW (argpool[in->v.c.argi + i]) && VHOMED (av)) {
			o_reg (&a, R_AX);
			o_bp (&b, vslot[av] + 2);
			mov_w (&a, &b);
			push (&a);
			o_reg (&a, R_AX);
			o_bp (&b, vslot[av]);
			mov_w (&a, &b);
			push (&a);
		} else {
			readop (av, &a, pc);
			push (&a);
		}
	}
	if (fi >= 0) {
		o_sym (&a, ins[fi].v.g.sym, 0);
		call (&a);
	} else {
		readop (in->v.c.a, &a, pc);
		call (&a);
	}
	if (in->v.c.n) {
		o_reg (&a, R_SP);
		o_imm (&b, bytes);
		alu ('a', SZ_W, &a, &b);
	}
	if (VDW (in->v.c.dst))
		storedw (in->v.c.dst, pc);
	else
		stohome (in->v.c.dst, R_AX, pc);
}

static void
genflow (in, pc)
struct insn *in;
int pc;
{
	struct opnd a, b;

	switch (in->op) {
	case 'R':
		if (VDW (in->v.R.a))
			loaddw (in->v.R.a, pc);
		else if (VHOMED (in->v.R.a)) {
			o_reg (&a, R_AX);
			o_bp (&b, vslot[in->v.R.a]);
			mov_w (&a, &b);
		} else if (VREG (in->v.R.a) != R_AX) {
			o_reg (&a, R_AX);
			o_reg (&b, VREG (in->v.R.a));
			mov_w (&a, &b);
		}
		jump (epilbl);
		break;
	case 'r':
		jump (epilbl);
		break;
	case 'L':
		label (in->v.L.lab);
		break;
	case 'J':
		edge (LARGI (in->v.L.argn), LN (in->v.L.argn),
		    in->v.L.lab, pc);
		jump (in->v.L.lab);
		break;
	case 'B':
		if (VDW (in->v.B.a)) {
			o_reg (&a, R_AX);
			o_bp (&b, vslot[in->v.B.a]);
			mov_w (&a, &b);
			o_reg (&a, R_AX);
			o_bp (&b, vslot[in->v.B.a] + 2);
			alu ('o', SZ_W, &a, &b);
			o_reg (&a, R_AX);
		} else {
			readop (in->v.B.a, &a, pc);
		}
		test_w (&a, &a);
		jcc (5, ilab (pc, 0));
		edge (BARGI (in->v.B.argn) + BN (in->v.B.argn),
		    BNF (in->v.B.argn), in->v.B.lab2, pc);
		jump (in->v.B.lab2);
		label (ilab (pc, 0));
		edge (BARGI (in->v.B.argn), BN (in->v.B.argn),
		    in->v.B.lab, pc);
		jump (in->v.B.lab);
		break;
	}
}

static void
gen (in, pc)
struct insn *in;
int pc;
{
	int i;

	for (i = 0; i < NREG; ++i)
		avoid[i] = 0;

	switch (in->op) {
	case 'i': case 'S': case 'g': case 'l': case 'C':
	case 'p': case 'q':
		genlit (in, pc);
		break;
	case '*': case '=':
		genst (in, pc);
		break;
	case 'f': case 's':
		genfar (in, pc);
		break;
	case 'u': case 'b':
		genarith (in, pc);
		break;
	case 'c':
		gencall (in, pc);
		break;
	case 'R': case 'r': case 'L': case 'J': case 'B':
		genflow (in, pc);
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

static int
maxlabel ()
{
	int i, m = 0;
	word l;

	for (i = 0; i < nins; ++i) {
		switch (ins[i].op) {
		case 'L':
		case 'J':
			l = ins[i].v.L.lab;
			break;
		case 'B':
			l = ins[i].v.B.lab2;
			if ((l & 0x8000) && (int) (l & 0x7fff) > m)
				m = l & 0x7fff;
			l = ins[i].v.B.lab;
			break;
		default:
			continue;
		}
		if ((l & 0x8000) && (int) (l & 0x7fff) > m)
			m = l & 0x7fff;
	}
	return m;
}

int
emit_func (sym, flags)
word sym;
byte flags;
{
	int i;
	struct opnd a, b;

	compute_intervals ();
	allocate ();
	find_saves ();
	intbase = maxlabel () + 1;
	epilbl = ilab (nins, 0);

	md_b ('F');
	md_b (flags);
	md_w (sym);
	for (i = 0; i < nargs; ++i)
		md_w (locsize[i]);
	md_w (0xffff);
	for (i = nargs; i < nloc; ++i)
		md_w (locsize[i]);
	md_w (0xffff);

	/* prologue */
	o_reg (&a, R_BP);
	push (&a);
	o_reg (&a, R_BP);
	o_reg (&b, R_SP);
	mov_w (&a, &b);
	for (i = 0; i < nsaves; ++i) {
		o_reg (&a, saved[i]);
		push (&a);
	}
	if (framesize) {
		o_reg (&a, R_SP);
		o_imm (&b, framesize);
		alu ('s', SZ_W, &a, &b);
	}

	for (i = 0; i < nins; ++i)
		gen (&ins[i], i);

	label (epilbl);
	/* saved[i] was pushed i-th, so it lives at [bp - 2*(i+1)] */
	for (i = nsaves - 1; i >= 0; --i) {
		o_reg (&a, saved[i]);
		o_bp (&b, -2 * (i + 1));
		mov_w (&a, &b);
	}
	o_reg (&a, R_SP);
	o_reg (&b, R_BP);
	mov_w (&a, &b);
	o_reg (&a, R_BP);
	pop (&a);
	ret ();

	md_b (0xff);
	md_w (framesize);
	md_b (nsaves);
	for (i = 0; i < nsaves; ++i)
		md_b (saved[i]);
	md_w (nloc);
	for (i = 0; i < nloc; ++i)
		md_w (locsite[i]);
	md_w (0xffff);
	md_b (0xff);
	return 0;
}
