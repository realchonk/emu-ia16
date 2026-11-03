/*
 * c2 optimization passes.
 */
#include <string.h>
#include "c2.h"

#define ISCONST(v)	(vsz[v] & 0x80)
#define CSIZE(v)	(vsz[v] & 3)

static int	block_start;

static word
maskof (size)
int size;
{
	return size == SZ_B ? 0xff : 0xffff;
}

static word
resolve (v)
word v;
{
	while (ren[v] != v) {
		ren[v] = ren[ren[v]];
		v = ren[v];
	}
	return v;
}

static void
setconst (idx, dst, size, val)
int idx;
word dst;
int size;
word val;
{
	int	j;

	for (j = block_start; j < idx; ++j)
		if (ins[j].op == 'i' && ins[j].v.i.size == size &&
		    ins[j].v.i.immhi == 0 &&
		    ins[j].v.i.imm == (val & maskof (size))) {
			ren[dst] = resolve (ins[j].v.i.dst);
			ins[idx].op = 0;
			return;
		}
	ins[idx].op = 'i';
	ins[idx].v.i.size = size;
	ins[idx].v.i.dst = dst;
	ins[idx].v.i.imm = val & maskof (size);
	ins[idx].v.i.immhi = 0;
	vsz[dst] = size | 0x80;
	kval[dst] = val & maskof (size);
}

static void
alias (idx, dst, src)
int idx;
word dst, src;
{
	if (CSIZE (dst) != CSIZE (src))
		return;
	ren[dst] = resolve (src);
	ins[idx].op = 0;
}

/* ---- optimization ----------------------------------------------------- */

static void
cse_i (idx, dst)
int idx;
word dst;
{
	int	j;

	for (j = block_start; j < idx; ++j)
		if (ins[j].op == 'i' && ins[j].v.i.size == ins[idx].v.i.size &&
		    ins[j].v.i.imm == ins[idx].v.i.imm &&
		    ins[j].v.i.immhi == ins[idx].v.i.immhi) {
			ren[dst] = resolve (ins[j].v.i.dst);
			ins[idx].op = 0;
			return;
		}
	if (ins[idx].v.i.size != SZ_D) {
		vsz[dst] = ins[idx].v.i.size | 0x80;
		kval[dst] = ins[idx].v.i.imm & maskof (ins[idx].v.i.size);
	}
}

static void
cse_S (idx, dst)
int idx;
word dst;
{
	int	j;

	for (j = block_start; j < idx; ++j)
		if (ins[j].op == 'S' && ins[j].v.S.six == ins[idx].v.S.six) {
			ren[dst] = resolve (ins[j].v.S.dst);
			ins[idx].op = 0;
			return;
		}
}

static void
cse_addr (idx, dst, local)
int idx, local;
word dst;
{
	int	j;
	word key;

	key = local ? ins[idx].v.l.slot : ins[idx].v.g.sym;
	for (j = block_start; j < idx; ++j) {
		if (ins[j].op != (local ? 'l' : 'g'))
			continue;
		if (local) {
			if (ins[j].v.l.slot != key)
				continue;
			ren[dst] = resolve (ins[j].v.l.dst);
		} else {
			if (ins[j].v.g.sym != key)
				continue;
			ren[dst] = resolve (ins[j].v.g.dst);
		}
		ins[idx].op = 0;
		return;
	}
}

static void
cse_p (idx, dst)
int idx;
word dst;
{
	int	j;

	for (j = block_start; j < idx; ++j)
		if (ins[j].op == ins[idx].op &&
		    ins[j].v.g.sym == ins[idx].v.g.sym) {
			ren[dst] = resolve (ins[j].v.g.dst);
			ins[idx].op = 0;
			return;
		}
}

static void
opt_C (idx)
int idx;
{
	struct insn *in;
	word a, dst, v, mask;
	int sa, sd;

	in = &ins[idx];
	a = resolve (in->v.C.a);
	dst = in->v.C.dst;
	in->v.C.a = a;
	sa = CSIZE (a);
	sd = CSIZE (dst);
	if (sa == sd) {
		alias (idx, dst, a);
		return;
	}
	if (ISCONST (a) && sa != SZ_D && sd != SZ_D) {
		mask = maskof (sa);
		v = kval[a] & mask;
		switch (in->v.C.fl) {
		case 0:
			v &= maskof (sd);
			break;
		case 1:
			v &= mask;
			break;
		case 2:
			if (sa == SZ_B && sd == SZ_W && (v & 0x80))
				v |= 0xff00;
			break;
		}
		setconst (idx, dst, sd, v);
		return;
	}
	/* CSE identical casts */
	{
		int j;
		for (j = block_start; j < idx; ++j)
			if (ins[j].op == 'C' && ins[j].v.C.fl == in->v.C.fl &&
			    resolve (ins[j].v.C.a) == a &&
			    CSIZE (ins[j].v.C.dst) == sd) {
				ren[dst] = resolve (ins[j].v.C.dst);
				ins[idx].op = 0;
				return;
			}
	}
}

static void
opt_u (idx)
int idx;
{
	struct insn *in;
	word a, dst, v, mask;
	int size;

	in = &ins[idx];
	a = resolve (in->v.u.a);
	dst = in->v.u.dst;
	in->v.u.a = a;
	size = in->v.u.size;
	if (ISCONST (a) && size != SZ_D) {
		mask = maskof (size);
		v = kval[a] & mask;
		if (in->v.u.bop == '-')
			v = (-v) & mask;
		else if (in->v.u.bop == '~')
			v = (~v) & mask;
		else
			v = v == 0 ? 1 : 0;
		setconst (idx, dst, size, v);
		return;
	}
	/* CSE */
	{
		int j;
		for (j = block_start; j < idx; ++j)
			if (ins[j].op == 'u' && ins[j].v.u.size == size &&
			    ins[j].v.u.bop == in->v.u.bop &&
			    resolve (ins[j].v.u.a) == a) {
				ren[dst] = resolve (ins[j].v.u.dst);
				ins[idx].op = 0;
				return;
			}
	}
}

static void
opt_b (idx)
int idx;
{
	struct insn *in;
	word a, b, dst, mask, v;
	int size, hasb, isimm, j, sgn;
	short sa, sb;

	in = &ins[idx];
	a = resolve (in->v.b.a);
	dst = in->v.b.dst;
	isimm = in->v.b.fl & 8;
	b = isimm ? in->v.b.bi : resolve (in->v.b.bi);
	in->v.b.a = a;
	in->v.b.bi = b;
	size = in->v.b.fl & 3;
	mask = maskof (size);
	sgn = (in->v.b.fl & 4) != 0;

	/* c3 can use an immediate only in the right operand.  For
	   commutative operations, move a constant left operand over. */
	if (size != SZ_D && !isimm && ISCONST (a) &&
	    b < MAXVAL && !ISCONST (b) &&
	    (in->v.b.bop == '+' || in->v.b.bop == '&' ||
	     in->v.b.bop == '|' || in->v.b.bop == '^' ||
	     in->v.b.bop == 'E' || in->v.b.bop == 'N')) {
		word olda = a;
		a = b;
		b = kval[olda] & mask;
		in->v.b.a = a;
		in->v.b.bi = b;
		in->v.b.fl |= 8;
		isimm = 1;
	}

	hasb = isimm || (b < MAXVAL && ISCONST (b));
	if (hasb && !isimm)
		b = kval[b] & mask;

	/* constant folding */
	if (size != SZ_D && hasb && ISCONST (a)) {
		word av = kval[a] & mask;
		int ok = 1;
		v = 0;
		sa = (short) av;
		sb = (short) b;
		if (size == SZ_B) {
			if (av & 0x80)
				sa = (short) (av | 0xff00);
			if (b & 0x80)
				sb = (short) (b | 0xff00);
		}
		switch (in->v.b.bop) {
		case '+': v = av + b; break;
		case '-': v = av - b; break;
		case '*': v = av * b; break;
		case '&': v = av & b; break;
		case '|': v = av | b; break;
		case '^': v = av ^ b; break;
		case '/':
		case '%':
			if (b == 0) { ok = 0; break; }
			if (sgn)
				v = in->v.b.bop == '/' ? sa / sb : sa % sb;
			else
				v = in->v.b.bop == '/' ? av / b : av % b;
			break;
		case 'l':
			if (b >= 16) { ok = 0; break; }
			v = av << b;
			break;
		case 'r':
			if (b >= 16) { ok = 0; break; }
			v = av >> b;
			break;
		case '<': v = sgn ? sa < sb : av < b; break;
		case '>': v = sgn ? sa > sb : av > b; break;
		case 'L': v = sgn ? sa <= sb : av <= b; break;
		case 'G': v = sgn ? sa >= sb : av >= b; break;
		case 'E': v = sgn ? sa == sb : av == b; break;
		case 'N': v = sgn ? sa != sb : av != b; break;
		default: ok = 0;
		}
		if (ok) {
			setconst (idx, dst, size, v & mask);
			return;
		}
	}

	/* algebraic identities using a known right operand */
	if (size != SZ_D && hasb) {
		if ((in->v.b.bop == '+' || in->v.b.bop == '-' ||
		     in->v.b.bop == '|' || in->v.b.bop == '^' ||
		     in->v.b.bop == 'l' || in->v.b.bop == 'r') && b == 0) {
			alias (idx, dst, a);
			return;
		}
		if (in->v.b.bop == '*' && b == 1) {
			alias (idx, dst, a);
			return;
		}
		if (in->v.b.bop == '&' && b == mask) {
			alias (idx, dst, a);
			return;
		}
		if (in->v.b.bop == '&' && b == 0) {
			setconst (idx, dst, size, 0);
			return;
		}
		if (in->v.b.bop == '*' && b == 0) {
			setconst (idx, dst, size, 0);
			return;
		}
		if (in->v.b.bop == '|' && b == mask) {
			setconst (idx, dst, size, mask);
			return;
		}
		if (!isimm &&
		    (in->v.b.bop == '-' || in->v.b.bop == '^' ||
		     in->v.b.bop == 'E' || in->v.b.bop == 'N') &&
		    a == resolve (in->v.b.bi)) {
			if (in->v.b.bop == 'E')
				setconst (idx, dst, size, 1);
			else if (in->v.b.bop == 'N')
				setconst (idx, dst, size, 0);
			else
				setconst (idx, dst, size, 0);
			return;
		}
	}

	/* strength-reduce constant multiply/divide/remainder */
	if (!isimm && hasb && size != SZ_D) {
		if (b == 1 && (in->v.b.bop == '/' || in->v.b.bop == '%')) {
			if (in->v.b.bop == '/')
				alias (idx, dst, a);
			else
				setconst (idx, dst, size, 0);
			return;
		}
		if (b != 0 && (b & (b - 1)) == 0 && b > 1 &&
		    (in->v.b.bop == '*' || in->v.b.bop == '/' ||
		     in->v.b.bop == '%')) {
			int sh = 0;
			word t = b;
			while (t > 1) {
				t >>= 1;
				++sh;
			}
			if (in->v.b.bop == '*') {
				in->v.b.bop = 'l';
				b = sh;
			} else if (in->v.b.bop == '/') {
				in->v.b.bop = 'r';
				b = sh;
			} else {
				in->v.b.bop = '&';
				b = b - 1;
			}
			in->v.b.bi = b;
		}
	}

	/* mark a constant RHS as an immediate where c3 supports it */
	if (size != SZ_D && hasb && !isimm &&
	    in->v.b.bop != '*' && in->v.b.bop != '/' &&
	    in->v.b.bop != '%') {
		in->v.b.fl |= 8;
		in->v.b.bi = b & mask;
		isimm = 1;
	}

	/* CSE identical pure binaries */
	for (j = block_start; j < idx; ++j) {
		if (ins[j].op != 'b')
			continue;
		if (ins[j].v.b.fl != in->v.b.fl)
			continue;
		if (ins[j].v.b.bop != in->v.b.bop)
			continue;
		if (resolve (ins[j].v.b.a) != a)
			continue;
		if (in->v.b.fl & 8) {
			if (ins[j].v.b.bi != in->v.b.bi)
				continue;
		} else if (resolve (ins[j].v.b.bi) != in->v.b.bi)
			continue;
		if (CSIZE (ins[j].v.b.dst) != CSIZE (dst))
			continue;
		ren[dst] = resolve (ins[j].v.b.dst);
		ins[idx].op = 0;
		return;
	}
}

/* store-to-load forwarding state */
static int	have_last;
static word	last_addr, last_src;
static int	last_size, last_idx;

static void
clear_last ()
{
	have_last = 0;
}

void
optimize ()
{
	int	i, k;
	word	w;
	struct insn *in;

	have_last = 0;
	/* initialize the rename map and constants */
	for (i = 0; i < (int) nval; ++i) {
		ren[i] = i;
		vsz[i] &= 3;
	}
	for (i = 0; i < nins; ++i)
		if (ins[i].op == 'i' && ins[i].v.i.size != SZ_D) {
			w = ins[i].v.i.dst;
			vsz[w] = ins[i].v.i.size | 0x80;
			kval[w] = ins[i].v.i.imm & maskof (ins[i].v.i.size);
		}

	/* ---- phase 1: local optimization ---- */
	block_start = 0;
	for (i = 0; i < nins; ++i) {
		in = &ins[i];
		if (in->op == 'L')
			block_start = i + 1;
		switch (in->op) {
		case 0:
			break;
		case 'i':
			cse_i (i, in->v.i.dst);
			break;
		case 'S':
			cse_S (i, in->v.S.dst);
			break;
		case 'g':
			cse_addr (i, in->v.g.dst, 0);
			break;
		case 'l':
			cse_addr (i, in->v.l.dst, 1);
			break;
		case 'C':
			opt_C (i);
			break;
		case 'u':
			opt_u (i);
			break;
		case 'b':
			opt_b (i);
			break;
		case '*': {
			word a = resolve (in->v.m.a);
			in->v.m.a = a;
			if (have_last && last_addr == a &&
			    last_size == in->v.m.size) {
				ren[in->v.m.dst] = last_src;
				ins[i].op = 0;
			} else
				clear_last ();
			break;
		}
		case '=': {
			word a = resolve (in->v.m.dst);
			word s = resolve (in->v.m.a);
			in->v.m.dst = a;
			in->v.m.a = s;
			if (have_last && last_addr == a &&
			    last_size == in->v.m.size) {
				ins[last_idx].op = 0;
				last_src = s;
				last_idx = i;
			} else {
				have_last = 1;
				last_addr = a;
				last_src = s;
				last_size = in->v.m.size;
				last_idx = i;
			}
			break;
		}
		case 'p':
		case 'q': {
			word s = resolve (in->v.g.sym);
			in->v.g.sym = s;
			cse_p (i, in->v.g.dst);
			break;
		}
		case 'f':
			in->v.m.a = resolve (in->v.m.a);
			clear_last ();
			break;
		case 's':
			in->v.m.dst = resolve (in->v.m.dst);
			in->v.m.a = resolve (in->v.m.a);
			clear_last ();
			break;
		case 'c':
			in->v.c.a = resolve (in->v.c.a);
			for (k = 0; k < (int) in->v.c.n; ++k) {
				w = argpool[in->v.c.argi + k];
				w = (w & 0x8000) | resolve (AVAL (w));
				argpool[in->v.c.argi + k] = w;
			}
			clear_last ();
			break;
		case 'R':
			in->v.R.a = resolve (in->v.R.a);
			clear_last ();
			break;
		case 'r':
			clear_last ();
			break;
		case 'L':
			clear_last ();
			break;
		case 'J':
			for (k = 0; k < (int) LN (in->v.L.argn); ++k)
				argpool[LARGI (in->v.L.argn) + k] =
				    resolve (argpool[LARGI (in->v.L.argn) + k]);
			clear_last ();
			break;
		case 'B': {
			word argn, lab, base;
			int n;

			in->v.B.a = resolve (in->v.B.a);
			argn = in->v.B.argn;
			for (k = 0; k < (int) BN (argn); ++k)
				argpool[BARGI (argn) + k] =
				    resolve (argpool[BARGI (argn) + k]);
			for (k = 0; k < (int) BNF (argn); ++k)
				argpool[BARGI (argn) + BN (argn) + k] =
				    resolve (argpool[BARGI (argn)
						     + BN (argn) + k]);
			if (ISCONST (in->v.B.a)) {
				if (kval[in->v.B.a]) {
					lab = in->v.B.lab;
					base = BARGI (argn);
					n = BN (argn);
				} else {
					lab = in->v.B.lab2;
					base = BARGI (argn) + BN (argn);
					n = BNF (argn);
				}
				in->op = 'J';
				in->v.L.lab = lab;
				in->v.L.argn = (base << 4) | n;
			}
			clear_last ();
			break;
		}
		}
		if (in->op == 'J' || in->op == 'B' ||
		    in->op == 'R' || in->op == 'r')
			block_start = i + 1;
	}

	/* ---- phase 2: dead code elimination ---- */
	for (;;) {
		int changed = 0;

		for (i = 0; i < (int) nval; ++i)
			ren[i] = 0;
		for (i = 0; i < nins; ++i) {
			in = &ins[i];
			switch (in->op) {
			case 'C': ren[in->v.C.a]++; break;
			case '*': ren[in->v.m.a]++; break;
			case '=': ren[in->v.m.dst]++; ren[in->v.m.a]++; break;
			case 'p':
			case 'q': ren[in->v.g.sym]++; break;
			case 'f': ren[in->v.m.a]++; break;
			case 's': ren[in->v.m.dst]++; ren[in->v.m.a]++; break;
			case 'u': ren[in->v.u.a]++; break;
			case 'b':
				ren[in->v.b.a]++;
				if (!(in->v.b.fl & 8))
					ren[in->v.b.bi]++;
				break;
			case 'c':
				ren[in->v.c.a]++;
				for (k = 0; k < (int) in->v.c.n; ++k)
					ren[AVAL (argpool[in->v.c.argi + k])]++;
				break;
			case 'R':
				ren[in->v.R.a]++;
				break;
			case 'J':
				for (k = 0; k < (int) LN (in->v.L.argn); ++k)
					ren[argpool[LARGI (in->v.L.argn) + k]]++;
				break;
			case 'B':
				ren[in->v.B.a]++;
				for (k = 0; k < (int) BN (in->v.B.argn); ++k)
					ren[argpool[BARGI (in->v.B.argn) + k]]++;
				for (k = 0; k < (int) BNF (in->v.B.argn); ++k)
					ren[argpool[BARGI (in->v.B.argn)
						    + BN (in->v.B.argn) + k]]++;
				break;
			}
		}
		for (i = 0; i < nins; ++i) {
			in = &ins[i];
			switch (in->op) {
			case 'i': w = in->v.i.dst; break;
			case 'S': w = in->v.S.dst; break;
			case 'g': w = in->v.g.dst; break;
			case 'l': w = in->v.l.dst; break;
			case 'C': w = in->v.C.dst; break;
			case '*': w = in->v.m.dst; break;
			case 'p':
			case 'q': w = in->v.g.dst; break;
			case 'u': w = in->v.u.dst; break;
			case 'b': w = in->v.b.dst; break;
			case 'f': w = in->v.m.dst; break;
			default: continue;
			}
			if (ren[w] == 0) {
				in->op = 0;
				changed = 1;
			}
		}
		if (!changed)
			break;
	}

	/* Remove a jump that falls through to the next label when its edge
	   arguments are already the label's parameters. */
	for (i = 0; i < nins; ++i) {
		int j, n, ok;

		if (ins[i].op != 'J')
			continue;
		for (j = i + 1; j < nins && ins[j].op == 0; ++j)
			;
		if (j >= nins || ins[j].op != 'L')
			continue;
		if (ins[j].v.L.lab != ins[i].v.L.lab)
			continue;
		n = LN (ins[i].v.L.argn);
		if (n != LN (ins[j].v.L.argn))
			continue;
		ok = 1;
		for (k = 0; k < n; ++k)
			if (argpool[LARGI (ins[i].v.L.argn) + k] !=
			    argpool[LARGI (ins[j].v.L.argn) + k]) {
				ok = 0;
				break;
			}
		if (ok)
			ins[i].op = 0;
	}
}
