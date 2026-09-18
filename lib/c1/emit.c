#include <stdbool.h>
#include <stdlib.h>
#include <assert.h>
#include <unistd.h>
#include <err.h>
#include "c1.h"

word	rpool[NRPOOL], soff;
size_t	nrpool;

static word
ralloc (size)
int size;
{
	int bc;

	if (nrpool >= NRPOOL)
		errx (1, "out of regs");

	switch (size) {
	case BYTE:
		bc = 1;
		break;
	case WORD:
		bc = 2;
		break;
	case DWORD:
		bc = 4;
		break;
	default:
		errx (1, "invalid size: %d", size);
	}

	rpool[nrpool] = (size << 14) | soff;
	soff += bc;
	return nrpool++;
}

static word
lalloc ()
{
	return maxlbl++;
}

static int
rsize (r)
word	r;
{
	assert (r < nrpool);
	return rpool[r] >> 14;
}

static int
tysize (ty)
word	ty;
{
	switch (ty & 3) {
	case 0:
		return BYTE;
	case 1:
		return WORD;
	case 2:
		return DWORD;
	default:
		errx (1, "called tysize() with block: %x", ty);
	}
}

static int
tysigned (ty)
word	ty;
{
	return !((ty >> 2) & 1);
}

static word
cast (ty, s)
word	ty, s;
{
	bool	sext;
	word	d;
	int	isz, osz;

	if (ty == (word)-1)
		return s;

	sext	= !((ty >> 2) & 1);
	isz	= rsize (s);
	osz	= tysize (ty);

	if (isz == osz)
		return s;

	d = ralloc (osz);

	putb ('C');
	putb (osz < isz ? 0 : (sext ? 2 : 1));
	putw (d);
	putw (s);
	return d;
}

static word
autocast (r)
word	r;
{
	return rsize (r) == BYTE ? cast (WORD, r) : r;
}

static word
load (sz, s)
int	sz;
word	s;
{
	word r;
	assert (rsize (s) == WORD);
	r = ralloc (sz);
	putb ('*');
	putb (sz);
	putw (r);
	putw (s);
	return r;
}

static void
puti (ty, x)
word	ty;
dword	x;
{
	switch (tysize (ty)) {
	case BYTE:
		putb (x);
		break;
	case WORD:
		putw (x);
		break;
	case DWORD:
		putd (x);
		break;
	default:
		abort ();
	}
}

static word
eexpr (ty, e)
word			 ty;
const struct expr	*e;
{
	struct expr	*e2;
	word		 d, s, t, u, c, ty2, args[MAXARGS], l1, l2, l3;
	byte		 flags;
	int		 i, sz;

	switch (e->e_type) {
	case 'i':
		d = ralloc (WORD);
		putb ('i');
		putb (WORD);
		putw (d);
		putw (e->e_i.i_v);
		break;
	case 'I':
		d = ralloc (DWORD);
		putb ('i');
		putb (DWORD);
		putw (d);
		putd (e->e_I.I_v);
		break;
	case 'S':
		d = ralloc (WORD);
		putb ('S');
		putw (d);
		putw (e->e_six);
		break;
	case '=':
		sz	= tysize (e->e_a.a_ty);
		s	= eexpr (e->e_a.a_ty, e->e_a.a_s);
		t	= eexpr (-1, e->e_a.a_d);
		putb ('=');
		putb (sz);
		putw (t);
		putw (s);
		d = load (sz, t);
		break;
	case 'l':
		d = ralloc (WORD);
		putb ('l');
		putw (d);
		putb (e->e_l.l_slot);
		break;
	case 'g':
		d = ralloc (WORD);
		putb ('g');
		putw (d);
		putw (e->e_g.g_sym);
		break;
	case '*':
		s = eexpr (WORD, e->e_d.d_e);
		d = load (tysize (e->e_d.d_ty), s);
		break;
	case 'u':
		sz = tysize (e->e_u.u_ty);
		s = eexpr (e->e_u.u_ty, e->e_u.u_e);
		d = ralloc (sz);
		putb ('u');
		putb (sz);
		putb (e->e_u.u_op);
		putw (d);
		putw (s);
		break;
	case 'b':
		ty2	= e->e_b.b_ty;
		sz	= tysize (ty2);
		s	= eexpr (ty2, e->e_b.b_l);
		t	= eexpr (ty2, e->e_b.b_r);
		d	= ralloc (sz);
		putb ('b');
		putb ((tysigned (ty) << 2) | sz);
		putb (e->e_b.b_op);
		putw (d);
		putw (s);
		putw (t);
		break;
	case 'c':
		for (e2 = e->e_c.c_a, i = 0; e2 != NULL; e2 = e2->e_next, ++i) {
			args[i] = autocast (eexpr (-1, e2));
		}

		s = eexpr (-1, e->e_c.c_f);
		switch (rsize (s)) {
		case BYTE:
			errx (1, "0x%zx: cannot call a byte", e->e_off);
		case WORD:
			flags = 0;
			break;
		case DWORD:
			errx (1, "0x%zx: far calls not implemented yet", e->e_off);
		default:
			abort ();
		}

		d = ralloc (tysize (e->e_c.c_ty));
		putb ('c');
		putb (flags);
		putb (e->e_c.c_n);
		putw (s);
		for (i = 0; i < e->e_c.c_n; ++i) {
			putb (rsize (args[i]));
			putw (args[i]);
		}
		break;
	case '?':
		c = eexpr (-1, e->e_t.t_c);
		l1 = lalloc ();
		l2 = lalloc ();
		l3 = lalloc ();

		/* B c, .Lt, .Lf */
		putb ('B');
		putw (c);
		putw (l1);
		putw (-1);
		putw (l2);
		putw (-1);

		/* .Lt: */
		putb ('L');
		putw (l1);
		putw (-1);
		s = eexpr (e->e_t.t_ty, e->e_t.t_t);
		putb ('J');
		putw (l3);
		putw (s);

		/* .Lf: */
		putb ('L');
		putw (l2);
		putw (-1);
		t = eexpr (e->e_t.t_ty, e->e_t.t_f);
		putb ('J');
		putw (l3);
		putw (s);

		/* .Ld: */
		d = ralloc (tysize (e->e_t.t_ty));
		putb ('L');
		putw (l3);
		putw (d);
		break;
	case 'n':
		ty2	= e->e_n.n_ty;
		sz	= tysize (ty2);
		s	= eexpr (ty2, e->e_n.n_e);
		t	= load (sz, s);
		u	= ralloc (sz);

		putb ('b');
		flags = sz | 8 | (!!tysigned (e->e_n.n_ty) << 2);
		putb (flags);
		putb (e->e_n.n_op);
		putw (u);
		putw (t);
		puti (ty2, (dword)e->e_n.n_scale);

		putb ('=');
		putb (sz);
		putw (s);
		putw (u);

		switch (e->e_n.n_when) {
		case 0: /* postfix */
			d = t;
			break;
		case 1: /* prefix */
			d = u;
			break;
		default:
			abort ();
		}

		break;
	default:
		errx (1, "0x%zx: unimplemented expr: %c (%x)", e->e_off, e->e_type, e->e_type);
	}

	return cast (ty, d);
}

void
estmt (s)
const struct stmt *s;
{
	word r;

	switch (s->s_type) {
	case 'r':
		putb ('r');
		break;
	case 'R':
		r = eexpr (s->s_R.R_ty, s->s_R.R_e);
		putb ('R');
		putw (r);
		break;
	case 'L':
	case 'J':
		putb (s->s_type);
		putw (0xffff);
		putw (s->s_sym);
		break;
	case 'B':
		r = eexpr (-1, s->s_B.B_c);
		putb ('B');
		putw (r);
		putw (s->s_B.B_t);
		putw (0xffff);
		putw (s->s_B.B_f);
		putw (0xffff);
		break;
	case 'E':
		eexpr (-1, s->s_e);
		break;
	default:
		errx (1, "unimplemented stmt: %x", s->s_type);
	}
}

