#include <stdbool.h>
#include <unistd.h>
#include <string.h>
#include <err.h>
#include "c1.h"

struct stmt	spool[NSPOOL];
struct expr	epool[NEPOOL];
word		lpool[NLPOOL];

static size_t	off;
static word	nreg, nspool, nepool, nlpool, narg;

static struct stmt *
salloc ()
{
	if (nspool >= NSPOOL)
		errx (1, "out of spool");
	return &spool[nspool++];
}

static struct expr *
ealloc ()
{
	if (nepool >= NEPOOL)
		errx (1, "out of epool");
	return &epool[nepool++];
}

static void
get (buf, num)
void	*buf;
int	 num;
{
	int n;

	n = read (0, buf, num);

	if (n == num) {
		off += num;
	} else if (n < 0) {
		err (1, "read()");
	} else {
		errx (1, "eof");
	}

}

static void
put (buf, num)
const void	*buf;
int		 num;
{
	int n;

	n = write (1, buf, num);

	if (n == num) {
		return;
	} else if (n < 0) {
		err (1, "write()");
	} else {
		errx (1, "no space");
	}
}

static byte
getb ()
{
	byte b;
	return get (&b, 1), b;
}

static word
getw ()
{
	word w;
	return get (&w, 2), w;
}

static dword
getd ()
{
	dword d;
	return get (&d, 4), d;
}

static void
putb (b)
byte b;
{
	put (&b, 1);
}

static void
putw (w)
word w;
{
	put (&w, 2);
}

static void
putd (d)
dword d;
{
	put (&d, 4);
}

static void
def ()
{
	byte	flags, ctype;
	word	sym, size, i;
	size_t	o;

	o = off;
	flags = getb ();
	sym = getw ();
	size = getw ();

	warnx ("0x%zx: D: flags=%x, sym=%u, size=%u", o, flags, sym, size);

	putb ('D');
	putb (flags);
	putw (sym);
	putw (size);

	if (!(flags & 0x04))
		return;

	while (1) {
		ctype = getb ();
		putb (ctype);
		switch (ctype) {
		case 0x00:
			return;

		case 0x01: /* wordn ... */
			for (i = getw (); i != 0; --i)
				putb (getb ());
			break;
		case 0x02: /* sym word-addend */
		case 0x03: /* six word-addend */
			putw (getw ());
			putw (getw ());
			break;
		default:
			errx (1, "0x%zx: D: invalid chunk: %x", off - 1, ctype);
		}
	}
}

static word
ty ()
{
	byte t;
	word n, o;

	o = off;
	t = getb ();
	if ((t & 3) != 3)
		return (word)t;

	n = getw ();
	if (n > 32767)
		errx (1, "0x%zx: ty: block too big: %u", o, n);

	return 0x8000 | n;
}

static struct expr *
expr ()
{
	struct expr	*e, *p;
	byte		 i;

	e = ealloc ();
	e->e_off	= off;
	e->e_type	= getb ();
	e->e_next	= NULL;

	warnx ("0x%zx: E: %c (%x)", e->e_off, e->e_type, e->e_type);

	switch (e->e_type) {
	case 'i':
		e->e_i.i_ty	= ty ();
		e->e_i.i_v	= getw ();
		break;
	case 'I':
		e->e_I.I_ty	= ty ();
		e->e_I.I_v	= getd ();
		break;
	case 'S':
		e->e_six	= getw ();
		break;
	case 'g':
		e->e_g.g_ty	= ty ();
		e->e_g.g_sym	= getw ();
		break;
	case 'l':
		e->e_l.l_ty	= ty ();
		e->e_l.l_slot	= getb ();
		break;
	case 'u':
		e->e_u.u_ty	= ty ();
		e->e_u.u_op	= getb ();
		e->e_u.u_e	= expr ();
		break;
	case 'b':
		e->e_b.b_ty	= ty ();
		e->e_b.b_op	= getb ();
		e->e_b.b_l	= expr ();
		e->e_b.b_r	= expr ();
		break;
	case '?':
		e->e_t.t_ty	= ty ();
		e->e_t.t_c	= expr ();
		e->e_t.t_t	= expr ();
		e->e_t.t_f	= expr ();
		break;
	case 'c':
		e->e_c.c_ty	= ty ();
		e->e_c.c_n	= getb ();
		e->e_c.c_f	= expr ();
		if (e->e_c.c_n == 0)
			break;

		p = e->e_c.c_a	= expr ();
		for (i = 1; i < e->e_c.c_n; ++i)
			p = (p->e_next = expr ());
		break;
	case '=':
		e->e_a.a_ty	= ty ();
		e->e_a.a_d	= expr ();
		e->e_a.a_s	= expr ();
		break;
	case 'n':
		e->e_n.n_ty	= ty ();
		e->e_n.n_op	= getb ();
		e->e_n.n_when	= getb ();
		e->e_n.n_scale	= getw ();
		e->e_n.n_e	= expr ();
		break;
	case ',':
		e->e_C.C_ty	= ty ();
		e->e_C.C_l	= expr ();
		e->e_C.C_r	= expr ();
		break;
	case '*':
		e->e_d.d_ty	= ty ();
		e->e_d.d_e	= expr ();
		break;
	default:
		errx (1, "0x%zx: E: invalid type=%x", e->e_off, e->e_type);
	}

	return e;
}

static struct stmt *
stmt ()
{
	struct stmt	*s;

	s = salloc ();
	s->s_off	= off;
	s->s_type	= getb ();
	s->s_next	= NULL;

	warnx ("0x%zx: S: %c (%x)", s->s_off, s->s_type, s->s_type);

	switch (s->s_type) {
	case 0xff:
	case 'r':
		break;
	case 'L':
	case 'J':
		s->s_sym = getw ();
		break;
	case 'R':
		s->s_R.R_ty = ty ();
		s->s_R.R_e = expr ();
		break;
	case 'B':
		s->s_B.B_t = getw ();
		s->s_B.B_f = getw ();
		s->s_B.B_c = expr ();
		break;
	case 'E':
		s->s_e = expr ();
		break;
	default:
		errx (1, "0x%zx: S: invalid type=%x", s->s_off, s->s_type);
	}

	return s;
}

static void
func ()
{
	struct stmt	*shead, *stail, *s;
	byte		 flags;
	word		 w, sym;
	size_t		 o;

	o = off;
	flags = getb ();
	sym = getw ();

	warnx ("0x%zx: F: flags=%x, sym=%u", o, flags, sym);

	putb ('F');
	putb (flags);
	putw (sym);

	if (!(flags & 0x04)) {
		if (getb () != 0xff)
			errx (1, "%zx: malformed F", o);
		putb (0xff);
		return;
	}

	nreg = 1;
	nspool = nepool = nlpool = narg = 0;

	/* code */
	shead = stail = stmt ();
	if (shead->s_type != 0xff) {
		do {
			s = stmt ();
			stail->s_next = s;
			stail = s;
		} while (s->s_type != 0xff);
	}

	/* args */
	while ((w = getw ()) != 0xffff)
		lpool[nlpool++] = w;
	narg = nlpool;

	/* vars */
	while ((w = getw ()) != 0xffff)
		lpool[nlpool++] = w;
}

static void
parse ()
{
	byte type;

	while (1) {
		switch (read (0, &type, 1)) {
		case 0:
			return;
		case 1:
			break;
		default:
			err (1, "read()");
		}

		switch (type) {
		case 'F':
			func ();
			break;
		case 'D':
			def ();
			break;
		default:
			errx (1, "0x%zx: invalid global: %x", off - 1, type);
		}
	}
}

int
main ()
{
	char buf[4];

	if (read (0, buf, 4) != 4)
		err (1, "fread()");

	if (memcmp (buf, "CIR", 4) != 0)
		errx (1, "invalid magic");

	off = 4;
	parse ();

	return 0;
}
