/*
 * ssa.c -- lower a c0 AST function body to the SSA form described in
 * bin/cc/ast-fmt.  Values are 0-based ids; control flow carries block
 * arguments instead of phi nodes.  The code stream is written to the
 * scratch file tfd as it is generated, because the value table (one entry
 * per value) has to follow it but is only complete once lowering is done;
 * the bytes are copied to the output afterwards.
 */
#include <string.h>
#include <unistd.h>
#include <err.h>
#include "c1.h"

/* ---- SSA value table -------------------------------------------------- */
static byte	vsz[NVALUE];		/* size code per value */
static word	nval;

/* ---- code stream (tfd) ------------------------------------------------ */
static size_t	clen;			/* bytes written for this function */

/* ---- synthetic label allocator ---------------------------------------- */
static word	nxtlbl;

/* ---- current block state ---------------------------------------------- */
static int	open;			/* a block is open (not terminated) */

/* from parse.c */
extern struct stmt	*shead;
extern word		 argbuf[], varbuf[];
extern size_t		 nargbuf, nvarbuf;

static word	eexpr ();

static void
ecb (b)
byte b;
{
	char c = b;

	if (write (tfd, &c, 1) != 1)
		err (1, "write()");
	++clen;
}

static void
ecw (w)
word w;
{
	ecb (w & 0xff);
	ecb ((w >> 8) & 0xff);
}

static void
ecd (d)
dword d;
{
	ecw ((word)(d & 0xffff));
	ecw ((word)((d >> 16) & 0xffff));
}

static word
newval (sz)
int sz;
{
	if (nval >= NVALUE)
		errx (1, "ssa: too many values");
	vsz[nval] = sz;
	return nval++;
}

static int
st (ty)
word ty;
{
	switch (ty & 3) {
	case 0:	return BYTE;
	case 1:	return WORD;
	case 2:	return DWORD;
	default: return WORD;		/* block == aggregate => its address */
	}
}

static int
signedty (ty)
word ty;
{
	return !((ty >> 2) & 1);
}

static word
newlbl ()
{
	return nxtlbl++;
}

static word
emit_int (ty, v)
word ty;
dword v;
{
	word d;
	int sz;

	sz = st (ty);
	d = newval (sz);
	ecb ('i');
	ecb (sz);
	ecw (d);
	if (sz == WORD)
		ecw ((word) v);
	else if (sz == BYTE)
		ecb ((byte) v);
	else
		ecd (v);
	return d;
}

static word
emit_g (sym)
word sym;
{
	word d;

	d = newval (WORD);
	ecb ('g');
	ecw (d);
	ecw (sym);
	return d;
}

static word
emit_s (six)
word six;
{
	word d;

	d = newval (WORD);
	ecb ('S');
	ecw (d);
	ecw (six);
	return d;
}

static word
emit_l (slot)
int slot;
{
	word d;

	d = newval (WORD);
	ecb ('l');
	ecw (d);
	ecb ((byte) slot);
	return d;
}

static word
emit_cast (ty, s)
word ty, s;
{
	word d;
	int isz, osz, flags;

	if (ty == (word) -1)
		return s;
	isz = vsz[s];
	osz = st (ty);
	if (isz == osz)
		return s;
	d = newval (osz);
	flags = osz < isz ? 0 : (signedty (ty) ? 2 : 1);
	ecb ('C');
	ecb (flags);
	ecw (d);
	ecw (s);
	return d;
}

static word
emit_un (sz, op, s)
int sz;
byte op;
word s;
{
	word d;

	d = newval (sz);
	ecb ('u');
	ecb (sz);
	ecb (op);
	ecw (d);
	ecw (s);
	return d;
}

static word
emit_bin (sz, sign, op, a, b)
int sz, sign;
byte op;
word a, b;
{
	word d;
	int flags;

	d = newval (sz);
	flags = (sz & 3) | (sign ? 4 : 0);
	ecb ('b');
	ecb (flags);
	ecb (op);
	ecw (d);
	ecw (a);
	ecw (b);
	return d;
}

static word
emit_load (sz, ai)
int sz;
word ai;
{
	word d;

	d = newval (sz);
	ecb ('*');
	ecb (sz);
	ecw (d);
	ecw (ai);
	return d;
}

static void
emit_store (sz, ai, si)
int sz;
word ai, si;
{
	ecb ('=');
	ecb (sz);
	ecw (ai);
	ecw (si);
}

/* close the current block with a fall-through jump if still open */
static void
close_block (lab)
word lab;
{
	if (open) {
		ecb ('J');
		ecw (lab);
		ecw (0);
		open = 0;
	}
}

static word
emit_label (lab, n)
word lab;
int n;
{
	word first;
	int i;

	ecb ('L');
	ecw (lab);
	ecw (n);
	first = nval;
	for (i = 0; i < n; ++i)
		ecw (newval (WORD));
	open = 1;
	return first;
}

static void
emit_jump (lab, n, a)
word lab;
int n;
word *a;
{
	int i;

	ecb ('J');
	ecw (lab);
	ecw (n);
	for (i = 0; i < n; ++i)
		ecw (a[i]);
	open = 0;
}

static void
emit_branch (c, tl, tn, ta, fl, fn, fa)
word c, tl;
int tn;
word *ta;
word fl;
int fn;
word *fa;
{
	int i;

	ecb ('B');
	ecw (c);
	ecw (tl);
	ecw (tn);
	for (i = 0; i < tn; ++i)
		ecw (ta[i]);
	ecw (fl);
	ecw (fn);
	for (i = 0; i < fn; ++i)
		ecw (fa[i]);
	open = 0;
}

/* widen a value to a word (for branching / block args) */
static word
asword (v)
word v;
{
	word z;

	if (vsz[v] == WORD)
		return v;
	z = newval (WORD);
	ecb ('C');
	ecb (1);			/* zero-extend */
	ecw (z);
	ecw (v);
	return z;
}

/* ---- expression lowering ---------------------------------------------- */

static byte
opcode (op)
int op;
{
	switch (op) {
	case 0x80: return 'E';
	case 0x81: return 'N';
	case 0x82: return 'L';
	case 0x83: return 'G';
	case 0x84: return 'l';
	case 0x85: return 'r';
	}
	return (byte) op;
}

static word
logic (e, isand)
struct expr *e;
int isand;
{
	word a, c, rhs, done, shortv, b, cb, one, zero, ta[1], fa[1];

	a = eexpr (-1, e->e_b.b_l);
	c = asword (a);
	rhs = newlbl ();
	done = newlbl ();

	shortv = emit_int (WORD, isand ? 0 : 1);
	ta[0] = shortv;
	/* if c is false, go straight to done with the short-circuit value */
	emit_branch (c, rhs, 0, (word *) 0, done, 1, ta);
	emit_label (rhs, 0);

	b = eexpr (-1, e->e_b.b_r);
	cb = asword (b);
	one = emit_int (WORD, 1);
	zero = emit_int (WORD, 0);
	ta[0] = one;
	fa[0] = zero;
	emit_branch (cb, done, 1, ta, done, 1, fa);
	return emit_label (done, 1);
}

static word
ternary (e)
struct expr *e;
{
	word c, thenl, elsel, donel, vt, vf, ta[1], fa[1];

	c = asword (eexpr (-1, e->e_t.t_c));
	thenl = newlbl ();
	elsel = newlbl ();
	donel = newlbl ();
	emit_branch (c, thenl, 0, (word *) 0, elsel, 0, (word *) 0);

	emit_label (thenl, 0);
	vt = asword (eexpr (e->e_t.t_ty, e->e_t.t_t));
	ta[0] = vt;
	emit_jump (donel, 1, ta);

	emit_label (elsel, 0);
	vf = asword (eexpr (e->e_t.t_ty, e->e_t.t_f));
	fa[0] = vf;
	emit_jump (donel, 1, fa);

	return emit_label (donel, 1);
}

static word
callx (e)
struct expr *e;
{
	struct expr *a;
	word argsv[MAXARGS];
	word f, d;
	byte n;
	int i;

	n = e->e_c.c_n;
	for (a = e->e_c.c_a, i = 0; i < n; ++i, a = a->e_next)
		argsv[i] = eexpr (-1, a);

	f = eexpr (-1, e->e_c.c_f);
	d = newval (st (e->e_c.c_ty));
	ecb ('c');
	ecb (st (e->e_c.c_ty));
	ecb (n);
	ecw (d);
	ecw (f);
	for (i = 0; i < n; ++i) {
		ecb (vsz[argsv[i]]);
		ecw (argsv[i]);
	}
	return d;
}

static word
incdec (e)
struct expr *e;
{
	struct expr *le;
	word addr, old, nv, ty2, t;
	int sz;

	le = e->e_n.n_e;
	ty2 = e->e_n.n_ty;
	sz = st (ty2);

	addr = le->e_type == '*' ? eexpr (WORD, le->e_d.d_e)
				 : eexpr (-1, le);
	old = emit_load (sz, addr);
	nv = emit_bin (sz, 0, (byte) e->e_n.n_op, old,
		       emit_int (WORD, e->e_n.n_scale));
	emit_store (sz, addr, nv);

	t = e->e_n.n_when ? nv : old;	/* prefix new, postfix old */
	return t;
}

static word
binexpr (e)
struct expr *e;
{
	word ty, a, b;
	int sz, sg;

	if (e->e_b.b_op == 0x86 || e->e_b.b_op == 0x36 ||
	    e->e_b.b_op == 0x87 || e->e_b.b_op == 0x37)
		return logic (e, e->e_b.b_op == 0x86 || e->e_b.b_op == 0x36);

	ty = e->e_b.b_ty;
	sz = st (ty);
	sg = signedty (ty);
	a = eexpr (ty, e->e_b.b_l);
	b = eexpr (ty, e->e_b.b_r);
	return emit_bin (sz, sg, opcode (e->e_b.b_op), a, b);
}

static word
eexpr (ty, e)
word ty;
struct expr *e;
{
	word d, ai, si;

	if (e == (struct expr *) 0)
		return emit_int (WORD, 0);

	switch (e->e_type) {
	case 'i':
		d = emit_int (e->e_i.i_ty, (dword) (word) e->e_i.i_v);
		break;
	case 'I':
		d = emit_int (e->e_I.I_ty, e->e_I.I_v);
		break;
	case 'S':
		d = emit_s (e->e_six);
		break;
	case 'g':
		d = emit_g (e->e_g.g_sym);
		break;
	case 'l':
		d = emit_l (e->e_l.l_slot);
		break;
	case '*':
		if (e->e_d.d_ty & 0x8000)
			d = eexpr (WORD, e->e_d.d_e);	/* array decays */
		else
			d = emit_load (st (e->e_d.d_ty),
				       eexpr (WORD, e->e_d.d_e));
		break;
	case '=':
		ai = eexpr (-1, e->e_a.a_d);
		si = eexpr (e->e_a.a_ty, e->e_a.a_s);
		emit_store (st (e->e_a.a_ty), ai, si);
		d = si;
		break;
	case 'u':
		d = emit_un (st (e->e_u.u_ty), (byte) e->e_u.u_op,
			     eexpr (e->e_u.u_ty, e->e_u.u_e));
		break;
	case 'b':
		d = binexpr (e);
		break;
	case '?':
		d = ternary (e);
		break;
	case 'c':
		d = callx (e);
		break;
	case 'n':
		d = incdec (e);
		break;
	case ',':
		eexpr (-1, e->e_C.C_l);
		d = eexpr (-1, e->e_C.C_r);
		break;
	default:
		errx (1, "0x%zx: unimplemented expr: %c", e->e_off, e->e_type);
	}
	return emit_cast (ty, d);
}

static void
emit_return (ty, e)
word ty;
struct expr *e;
{
	word r;

	if (e == (struct expr *) 0) {
		ecb ('r');
		open = 0;
		return;
	}
	r = eexpr (ty, e);		/* evaluate first: it emits instructions */
	ecb ('R');
	ecw (r);
	open = 0;
}

static void
estmt (s)
struct stmt *s;
{
	word r;

	switch (s->s_type) {
	case 'r':
		emit_return (-1, (struct expr *) 0);
		break;
	case 'R':
		emit_return (s->s_R.R_ty, s->s_R.R_e);
		break;
	case 'L':
		close_block (s->s_sym);
		emit_label (s->s_sym, 0);
		break;
	case 'J':
		emit_jump (s->s_sym, 0, (word *) 0);
		break;
	case 'B':
		r = asword (eexpr (-1, s->s_B.B_c));
		emit_branch (r, s->s_B.B_t, 0, (word *) 0,
			     s->s_B.B_f, 0, (word *) 0);
		break;
	case 'E':
		eexpr (-1, s->s_e);
		break;
	default:
		errx (1, "0x%zx: unimplemented stmt: %c", s->s_off, s->s_type);
	}
}

/* ---- driver ----------------------------------------------------------- */
void
ssa_func (flags, sym)
byte flags;
word sym;
{
	struct stmt *s;
	size_t i, n;
	char buf[128];

	putb ('F');
	putb (flags);
	putw (sym);

	for (i = 0; i < nargbuf; ++i)
		putw (argbuf[i]);
	putw (0xffff);
	for (i = 0; i < nvarbuf; ++i)
		putw (varbuf[i]);
	putw (0xffff);

	clen = 0;
	nval = 0;
	nxtlbl = maxlbl + 1;
	open = 0;

	if (lseek (tfd, 0L, SEEK_SET) != 0)
		err (1, "lseek()");

	for (s = shead; s->s_type != 0xff; s = s->s_next)
		estmt (s);

	if (open)
		ecb ('r');		/* implicit fall-through return */

	/* copy the code out of the scratch file, then append the table */
	if (lseek (tfd, 0L, SEEK_SET) != 0)
		err (1, "lseek()");
	for (n = clen; n > 0; n -= i) {
		i = n < sizeof buf ? n : sizeof buf;
		if (read (tfd, buf, i) != (int) i)
			err (1, "read()");
		if (write (ofd, buf, i) != (int) i)
			err (1, "write()");
	}
	putb (0xff);

	for (i = 0; i < nval; ++i)
		putw ((word) ((vsz[i] & 3) << 14));
	putw (0xffff);

	putb (0xff);
}
