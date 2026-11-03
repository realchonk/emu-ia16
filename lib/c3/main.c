/*
 * c3 -- SSA (CIR\001, see bin/cc/ast-fmt) -> machine-dependent IR
 * (CIR\002, *.mdi).
 *
 * Reads the SSA on stdin and writes MDIR to stdout.  Register allocation is
 * a linear scan over conservative live intervals (first def .. last use,
 * extended across block boundaries); values that do not fit spill to
 * bp-relative slots.  The ABI is cdecl: arguments on the stack with the
 * first at [bp+4], caller cleans up, result in AX, SI/DI callee-saved.
 */
#include <string.h>
#include <unistd.h>
#include <fcntl.h>
#include <stdlib.h>
#include <err.h>
#include "c3.h"

int	strfd, symfd;
size_t	off;

/* ---- buffered output -------------------------------------------------- */
static byte	obuf[256];
static int	ocnt;

void
md_flush ()
{
	if (ocnt != 0) {
		if (write (1, obuf, ocnt) != ocnt)
			err (1, "write()");
		ocnt = 0;
	}
}

void
md_b (b)
int b;
{
	if (ocnt >= (int) sizeof obuf)
		md_flush ();
	obuf[ocnt++] = b;
}

void
md_w (w)
int w;
{
	md_b (w & 0xff);
	md_b ((w >> 8) & 0xff);
}

/* ---- input helpers ---------------------------------------------------- */
void
get (buf, num)
void	*buf;
int	 num;
{
	int n;

	n = read (0, buf, num);
	if (n == num)
		off += num;
	else if (n < 0)
		err (1, "read()");
	else
		errx (1, "c3: eof");
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

/* ---- SSA image of one function ---------------------------------------- */
struct insn	ins[MAXINS];
int		nins;
word		nval;			/* number of SSA values */
word		argpool[ARGPOOL];
int		nargpool;
byte		vdw[(MAXVAL + 7) / 8];
word		locsize[MAXLOC];	/* byte size of each local slot */
int		nloc;
int		nargs;

int		emit_func ();		/* emit.c */

static void
pool_args (n)
int n;
{
	if (nargpool + n > ARGPOOL)
		errx (1, "c3: too many pooled args");
}

/* ---- data declarations ------------------------------------------------ */
static void
def ()
{
	byte	flags, cnk;
	word	sym, sym2, size, j, n;

	flags = getb ();
	sym = getw ();
	size = getw ();

	md_b ('D');
	md_b (flags);
	md_w (sym);
	md_w (size);

	if ((flags & 4) == 0)		/* .bss: no initialiser bytes */
		return;

	for (;;) {
		cnk = getb ();
		md_b (cnk);
		if (cnk == 0)
			return;
		switch (cnk) {
		case 0x01:
			n = getw ();
			md_w (n);
			for (j = 0; j < n; ++j)
				md_b (getb ());
			break;
		case 0x02:
		case 0x03:
			sym2 = getw ();
			n = getw ();
			md_w (sym2);
			md_w (n);
			break;
		default:
			errx (1, "c3: bad init chunk %#x", cnk);
		}
	}
}

/* ---- function body ---------------------------------------------------- */
static void
func ()
{
	byte	flags;
	word	sym, w;
	int	i, k;

	flags = getb ();
	sym = getw ();

	if ((flags & 4) == 0) {
		if (getb () != 0xff)
			errx (1, "c3: malformed F");
		md_b ('F');
		md_b (flags);
		md_w (sym);
		md_b (0xff);
		return;
	}

	nloc = 0;
	nargs = 0;
	while ((w = getw ()) != 0xffff)		/* arg sizes */
		if (nloc < MAXLOC) {
			locsize[nloc++] = w;
			++nargs;
		} else
			errx (1, "c3: too many locals");
	while ((w = getw ()) != 0xffff)		/* var sizes */
		if (nloc < MAXLOC)
			locsize[nloc++] = w;
		else
			errx (1, "c3: too many locals");

	nins = 0;
	nargpool = 0;
	memset (vdw, 0, sizeof vdw);
	while (1) {
		i = nins;
		if (i >= MAXINS)
			errx (1, "c3: too many instructions (%d used)", i);
		ins[i].op = getb ();
		if (ins[i].op == 0xff)
			break;
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
		case '*':
			ins[i].v.m.size = getb ();
			ins[i].v.m.dst = getw ();
			ins[i].v.m.a = getw ();
			break;
		case '=':
			ins[i].v.m.size = getb ();
			ins[i].v.m.dst = getw ();	/* address */
			ins[i].v.m.a = getw ();	/* value */
			break;
		case 'p':
		case 'q':
			ins[i].v.g.dst = getw ();
			ins[i].v.g.sym = getw ();	/* near address value */
			break;
		case 'f':
			ins[i].v.m.size = getb ();
			ins[i].v.m.dst = getw ();
			ins[i].v.m.a = getw ();	/* far address value */
			break;
		case 's':
			ins[i].v.m.size = getb ();
			ins[i].v.m.dst = getw ();	/* far address value */
			ins[i].v.m.a = getw ();	/* value */
			break;
		case 'u':
			ins[i].v.u.size = getb ();
			ins[i].v.u.bop = getb ();	/* op char */
			ins[i].v.u.dst = getw ();
			ins[i].v.u.a = getw ();
			break;
		case 'b':
			ins[i].v.b.fl = getb ();
			ins[i].v.b.bop = getb ();
			ins[i].v.b.dst = getw ();
			ins[i].v.b.a = getw ();
			ins[i].v.b.bi = getw ();
			break;
		case 'c':
			ins[i].v.c.size = getb ();
			ins[i].v.c.n = getb ();
			ins[i].v.c.dst = getw ();
			ins[i].v.c.a = getw ();	/* function address */
			ins[i].v.c.argi = nargpool;
			pool_args ((int) ins[i].v.c.n);
			for (k = 0; k < (int) ins[i].v.c.n; ++k) {
				byte asz = getb ();
				word av = getw ();
				argpool[nargpool++] = av
				    | (asz == SZ_D ? 0x8000 : 0);
			}
			break;
		case 'R':
			ins[i].v.R.a = getw ();
			break;
		case 'r':
			break;
		case 'L':
		case 'J': {
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
			break;
		}
		case 'B': {
			word n, nf, base;

			ins[i].v.B.a = getw ();	/* cond */
			ins[i].v.B.lab = getw ();
			n = getw ();
			if (n > 15)
				errx (1, "c3: too many block args");
			base = nargpool;
			if (base > 511)
				errx (1, "c3: too many pooled args");
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
			break;
		}
		default:
			errx (1, "c3: bad stmt %c (%#x)", ins[i].op, ins[i].op);
		}
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

	emit_func (sym, flags);
}

void
parse ()
{
	byte	type;

	if (lseek (0, 0L, SEEK_SET) != 0)
		err (1, "lseek");
	{
		char buf[4];
		get (buf, 4);
		if (memcmp (buf, "CIR\001", 4) != 0)
			errx (1, "c3: invalid magic");
	}
	while (read (0, &type, 1) == 1) {
		switch (type) {
		case 'D':	def (); break;
		case 'F':	func (); break;
		default:	errx (1, "c3: invalid global %c", type);
		}
	}
}

static void
strtable ()
{
	byte	ch;
	long	n, i;

	n = lseek (strfd, 0L, SEEK_END);
	if (n < 0)
		err (1, "lseek");
	if (lseek (strfd, 0L, SEEK_SET) < 0)
		err (1, "lseek");
	if (n > 0xffff)
		errx (1, "c3: string table too large");
	md_b ('T');
	md_w ((word) n);
	for (i = 0; i < n; ++i) {
		if (read (strfd, &ch, 1) != 1)
			err (1, "read()");
		md_b (ch);
	}
}

int
main (argc, argv)
int	 argc;
char	**argv;
{
	if (argc != 3)
		errx (1, "usage: /lib/c3 strfile symfile");

	strfd = open (argv[1], O_RDONLY);
	if (strfd < 0)
		err (1, "open(%s)", argv[1]);
	symfd = open (argv[2], O_RDONLY);
	if (symfd < 0)
		err (1, "open(%s)", argv[2]);

	md_b ('C'); md_b ('I'); md_b ('R'); md_b (2);
	strtable ();
	parse ();
	md_flush ();

	close (strfd);
	close (symfd);
	return 0;
}
