/*
 * c3a -- register allocation half of c3.
 *
 * Reads SSA (CIR\001) on stdin and writes the allocation stream
 * (CIR\003) on stdout.  See emit.c for the second half.
 */
#include <string.h>
#include <unistd.h>
#include <err.h>
#include "c3.h"

static word	defpc[MAXVAL];
static int	ivlo[MAXVAL], ivhi[MAXVAL];

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
	if (lo >= 0 && hi == lo)
		defpc[v] = lo;
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
scan_args (i)
int i;
{
	int k;

	switch (ins[i].op) {
	case 'c':
		touch (ins[i].v.c.dst, i, i);
		touch (ins[i].v.c.a, -1, i);
		for (k = 0; k < ins[i].v.c.n; ++k)
			use (AVAL (argpool[ins[i].v.c.argi + k]), -1, i);
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

static void
compute_intervals ()
{
	int i;
	word v;

	for (v = 0; v < nval; ++v) {
		ivlo[v] = 0x7fff;
		ivhi[v] = -1;
		defpc[v] = 0xffff;
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
			scan_args (i);
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
		case 'B':
		case 'L':
			scan_args (i);
			break;
		}
	}
	for (v = 0; v < nval; ++v)
		if (ivlo[v] > ivhi[v])
			ivlo[v] = ivhi[v] = 0;
}

#define VDIR_LOCAL	1
#define VDIR_GLOBAL	2
#define VDIR_CALL	3

static void
nodir (v)
word v;
{
	if (v < nval)
		vdir[v] = 0;
}

/*
 * Address values that are only ever used as a load/store address (local
 * or global) or as a direct call target do not need to be materialised.
 */
static void
direct_args (i)
int i;
{
	int k;

	switch (ins[i].op) {
	case 'J':
		for (k = 0; k < (int) LN (ins[i].v.L.argn); ++k)
			nodir (argpool[LARGI (ins[i].v.L.argn) + k]);
		break;
	case 'B':
		nodir (ins[i].v.B.a);
		for (k = 0; k < (int) BN (ins[i].v.B.argn); ++k)
			nodir (argpool[BARGI (ins[i].v.B.argn) + k]);
		for (k = 0; k < (int) BNF (ins[i].v.B.argn); ++k)
			nodir (argpool[BARGI (ins[i].v.B.argn)
			    + BN (ins[i].v.B.argn) + k]);
		break;
	}
}

static void
local_direct ()
{
	int i, k;
	word v;

	for (v = 0; v < nval; ++v)
		vdir[v] = 0;
	for (i = 0; i < nins; ++i) {
		if (ins[i].op == 'l')
			vdir[ins[i].v.l.dst] |= 1;
		else if (ins[i].op == 'g')
			vdir[ins[i].v.g.dst] |= 2 | 4;
	}
	for (i = 0; i < nins; ++i) {
		switch (ins[i].op) {
		case 'c':
			if (ins[i].v.c.a < nval)
				vdir[ins[i].v.c.a] &= ~3;
			for (k = 0; k < (int) ins[i].v.c.n; ++k)
				nodir (AVAL (argpool[ins[i].v.c.argi + k]));
			break;
		case 'C':
			nodir (ins[i].v.C.a);
			break;
		case '*':
			if (ins[i].v.m.a < nval)
				vdir[ins[i].v.m.a] &= ~4;
			break;
		case '=':
			if (ins[i].v.m.dst < nval)
				vdir[ins[i].v.m.dst] &= ~4;
			nodir (ins[i].v.m.a);
			break;
		case 'p':
		case 'q':
			nodir (ins[i].v.g.sym);
			break;
		case 'f':
			nodir (ins[i].v.m.a);
			break;
		case 's':
			nodir (ins[i].v.m.dst);
			nodir (ins[i].v.m.a);
			break;
		case 'u':
			nodir (ins[i].v.u.a);
			break;
		case 'b':
			nodir (ins[i].v.b.a);
			if (!(ins[i].v.b.fl & 8))
				nodir (ins[i].v.b.bi);
			break;
		case 'R':
			nodir (ins[i].v.R.a);
			break;
		case 'J':
		case 'B':
			direct_args (i);
			break;
		}
	}
	for (i = 0; i < nins; ++i) {
		if (ins[i].op == 'l') {
			v = ins[i].v.l.dst;
			if (vdir[v] & 1) {
				vdir[v] = VDIR_LOCAL;
				vslot[v] = locsite[ins[i].v.l.slot];
			} else
				vdir[v] = 0;
		} else if (ins[i].op == 'g') {
			v = ins[i].v.g.dst;
			if (vdir[v] & 2) {
				vdir[v] = VDIR_GLOBAL;
				vslot[v] = ins[i].v.g.sym;
			} else if (vdir[v] & 4)
				vdir[v] = VDIR_CALL;
			else
				vdir[v] = 0;
		}
	}
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
layout_frame ();

static void
allocate ()
{
	int i, j, t, cand, victim, vend, norder, bestlo;
	int di, coal;
	word v, s;
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
		coal = 0;
		di = defpc[v];
		if (di != 0xffff) {
			switch (ins[di].op) {
			case 'C': s = ins[di].v.C.a; break;
			case 'u': s = ins[di].v.u.a; break;
			case 'b': s = ins[di].v.b.a; break;
			case '*':
				s = ins[di].v.m.size == SZ_B
				    ? 0xffff : ins[di].v.m.a;
				break;
			default: s = 0xffff;
			}
			if (s < nval && s != v && !VHOMED (s) &&
			    ivhi[s] == di && owner[VREG (s)] == s &&
			    allowed (v, VREG (s)) &&
			    (!crosses_call (v) || VREG (s) == R_SI ||
			     VREG (s) == R_DI)) {
				cand = VREG (s);
				coal = 1;
				for (j = 0; j < nactive; ++j)
					if (active[j] == s) {
						active[j] = active[--nactive];
						break;
					}
				owner[cand] = v;
			}
		}
		if (!coal)
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

	layout_frame ();
}

static void
layout_frame ()
{
	int i, j, src;
	word v;

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

static void
compute_busybits ()
{
	int p;
	word v;
	byte bit;

	for (p = 0; p < nins; ++p)
		busybits[p] = 0;
	for (v = 0; v < nval; ++v) {
		if (VHOMED (v))
			continue;
		bit = 1 << VREG (v);
		for (p = ivlo[v]; p <= ivhi[v] && p < nins; ++p)
			busybits[p] |= bit;
	}
}

void
alloc_func (flags, sym)
byte flags;
word sym;
{
	word v;
	int i;

	compute_intervals ();
	allocate ();
	local_direct ();
	compute_busybits ();
	/* SI and DI are callee-saved and may be used for values or scratch,
	   so always preserve them (they sit at [bp-2] and [bp-4]). */
	saved[0] = R_SI;
	saved[1] = R_DI;
	nsaves = 2;

	md_b ('F');
	md_b (flags);
	md_w (sym);
	md_w (nloc);
	md_w (nargs);
	raw (locsize, nloc * sizeof locsize[0]);

	md_w (nins);
	raw (ins, nins * (int) sizeof (struct insn));

	md_w (nargpool);
	raw (argpool, nargpool * sizeof argpool[0]);

	md_w (nval);
	raw (vdw, VDWBYTES);

	md_w (framesize);
	md_b (nsaves);
	raw (saved, nsaves * sizeof saved[0]);
	md_w (nloc);
	raw (locsite, nloc * sizeof locsite[0]);
	md_w (nval);
	raw (busybits, nins);
	raw (vinfo, nval);
	raw (vslot, nval * sizeof vslot[0]);
	raw (vdir, nval);
	(void) i;
	(void) v;
}
