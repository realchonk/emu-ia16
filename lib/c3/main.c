/*
 * c3 -- SSA (CIR\001, see bin/cc/ast-fmt) -> 16-bit x86 assembly.
 *
 * Reads the SSA on stdin and writes NASM to stdout.  Register allocation is
 * a linear scan over conservative live intervals (first def .. last use,
 * extended across block boundaries); values that do not fit spill to
 * bp-relative slots.  The ABI is cdecl: arguments on the stack with the
 * first at [bp+4], caller cleans up, result in AX, SI/DI callee-saved.
 */
#include <string.h>
#include <unistd.h>
#include <fcntl.h>
#include <stdlib.h>
#include <stdio.h>
#include <err.h>
#include "c3.h"

int	strfd, symfd;
size_t	off;

void
rewind_input ()
{
	if (lseek (0, 0L, SEEK_SET) != 0)
		err (1, "lseek");
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

static void
putb (b)
byte b;
{
	printf ("\tdb %u\n", (unsigned) b);
}

/* Name of the function currently being emitted; generated labels are
   qualified with it so they do not depend on NASM's positional scoping. */
char	funcname[64] = "toplevel";

const char *
getsym (sym)
word sym;
{
	static char buf[96];

	if (sym & 0x8000) {
		/* Generated label: qualify it.  A leading `$` still escapes
		   any NASM mnemonic. */
		snprintf (buf, sizeof buf, "$%s.L%u", funcname,
			(unsigned) (sym & 0x7fff));
	} else {
		int n = pread (symfd, buf + 1, sizeof buf - 2, (long) sym);
		if (n < 0)
			n = 0;
		buf[0] = '$';
		buf[n + 1] = '\0';
		buf[strnlen (buf + 1, sizeof buf - 1) + 1] = '\0';
	}
	return buf;
}

/* ---- SSA image of one function ---------------------------------------- */
struct insn	ins[MAXINS];
int		nins;
word		nval;			/* number of SSA values */
byte		vsz[MAXVAL];		/* size code per value */
word		argpool[ARGPOOL];
int		nargpool;
word		locsize[MAXLOC];	/* byte size of each local slot */
int		nloc;
int		nargs;

int		emit_func ();		/* emit.c */

/* Names of symbols defined in this translation unit, so an undefined `call`
   target or `g` address can be declared extern. */
#define MAXDEF	256
word		defs[MAXDEF];
int		ndefs;

/* ---- data declarations ------------------------------------------------ */
static void
def ()
{
	byte	flags, cnk;
	word	sym, sym2, size, i, j, n;
	int	sc;

	flags = getb ();
	sym = getw ();
	size = getw ();
	sc = flags & 3;

	if (sc == SC_EXTERN) {
		printf ("extern %s\n", getsym (sym));
		return;
	}

	printf ("section %s\n", (flags & 4) ? ".data" : ".bss");
	if (sc == SC_GLOBAL)
		printf ("global %s\n", getsym (sym));
	printf ("%s:\n", getsym (sym));

	if ((flags & 4) == 0) {
		printf ("\tresb %u\n", (unsigned) size);
		return;
	}

	for (i = 0; (cnk = getb ()) != 0; ) {
		switch (cnk) {
		case 0x01:
			n = getw ();
			for (j = 0; j < n; ++i, ++j)
				putb (getb ());
			break;
		case 0x02:
			sym2 = getw ();
			n = getw ();
			printf ("\tdw %s + %u\n", getsym (sym2), (unsigned) n);
			break;
		case 0x03:
			sym2 = getw ();
			n = getw ();
			printf ("\tdw strtab + %u + %u\n",
				(unsigned) sym2, (unsigned) n);
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
		(void) getb ();			/* 0xff */
		printf ("extern %s\n", getsym (sym));
		return;
	}

	nloc = 0;
	nargs = 0;
	while ((w = getw ()) != 0xffff)		/* arg sizes */
		if (nloc < MAXLOC) {
			locsize[nloc++] = w;
			++nargs;
		}
	while ((w = getw ()) != 0xffff)		/* var sizes */
		if (nloc < MAXLOC)
			locsize[nloc++] = w;

	nins = 0;
	nargpool = 0;
	while (1) {
		i = nins;
		if (i >= MAXINS)
			errx (1, "c3: too many instructions");
		ins[i].op = getb ();
		if (ins[i].op == 0xff)
			break;
		switch (ins[i].op) {
		case 'i':
			ins[i].v.i.size = getb ();
			ins[i].v.i.dst = getw ();
			if (ins[i].v.i.size == SZ_B)
				ins[i].v.i.imm = getb ();
			else if (ins[i].v.i.size == SZ_W)
				ins[i].v.i.imm = getw ();
			else
				ins[i].v.i.immd = getd ();
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
		case 'u':
			ins[i].v.u.size = getb ();
			ins[i].v.u.bop = getb ();	/* op char */
			ins[i].v.u.dst = getw ();
			ins[i].v.u.a = getw ();
			break;
		case 'b':
			ins[i].v.b.fl = getb ();
			ins[i].v.b.size = ins[i].v.b.fl & 3;
			ins[i].v.b.bop = getb ();
			ins[i].v.b.dst = getw ();
			ins[i].v.b.a = getw ();
			ins[i].v.b.b = getw ();
			break;
		case 'c':
			ins[i].v.c.size = getb ();
			ins[i].v.c.n = getb ();
			ins[i].v.c.dst = getw ();
			ins[i].v.c.a = getw ();	/* function address */
			ins[i].v.c.argi = nargpool;
			for (k = 0; k < ins[i].v.c.n; ++k) {
				(void) getb ();
				argpool[nargpool++] = getw ();
			}
			break;
		case 'R':
			ins[i].v.R.a = getw ();
			break;
		case 'r':
			break;
		case 'L':
			ins[i].v.L.lab = getw ();
			ins[i].v.L.n = getw ();
			ins[i].v.L.argi = nargpool;
			for (k = 0; k < ins[i].v.L.n; ++k)
				argpool[nargpool++] = getw ();
			break;
		case 'J':
			ins[i].v.L.lab = getw ();
			ins[i].v.L.n = getw ();
			ins[i].v.L.argi = nargpool;
			for (k = 0; k < ins[i].v.L.n; ++k)
				argpool[nargpool++] = getw ();
			break;
		case 'B':
			ins[i].v.B.a = getw ();	/* cond */
			ins[i].v.B.lab = getw ();
			ins[i].v.B.n = getw ();
			ins[i].v.B.argi = nargpool;
			for (k = 0; k < ins[i].v.B.n; ++k)
				argpool[nargpool++] = getw ();
			ins[i].v.B.lab2 = getw ();
			ins[i].v.B.nfalse = getw ();
			ins[i].v.B.fargi = nargpool;
			for (k = 0; k < ins[i].v.B.nfalse; ++k)
				argpool[nargpool++] = getw ();
			break;
		default:
			errx (1, "c3: bad stmt %c (%#x)", ins[i].op, ins[i].op);
		}
		++nins;
	}

	nval = 0;
	while ((w = getw ()) != 0xffff) {
		if (nval >= MAXVAL)
			errx (1, "c3: too many values");
		vsz[nval++] = (w >> 14) & 3;
	}
	if (getb () != 0xff)
		errx (1, "c3: malformed F");

	emit_func (sym, flags);
}

void
parse ()
{
	byte	type;

	rewind_input ();
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

/* ---- symbol discovery pre-pass ---------------------------------------- */
/* Walk the SSA once (via lseek-rewind) recording defined symbols (D and
   defined F) and every `g` address, so externs can be declared. */
static word	refs[512];
static int	nrefs;

static void
skip_strings ()
{
	word w;

	while ((w = getw ()) != 0xffff)
		;
}

static void
scan_code ()
{
	byte t;

	for (;;) {
		t = getb ();
		if (t == 0xff)
			return;
		switch (t) {
		case 'i': {
			byte sz = getb ();
			(void) getw ();
			if (sz == 0) (void) getb ();
			else if (sz == 1) (void) getw ();
			else (void) getd ();
			break;
		}
		case 'I': (void) getw (); (void) getd (); break;
		case 'S': (void) getw (); (void) getw (); break;
		case 'g': {
			word rsym;
			(void) getw ();
			rsym = getw ();
			if (nrefs < 512)
				refs[nrefs++] = rsym;
			break;
		}
		case 'l': (void) getw (); (void) getb (); break;
		case 'C': (void) getb (); (void) getw (); (void) getw (); break;
		case '*': case '=':
			(void) getb (); (void) getw (); (void) getw (); break;
		case 'u':
			(void) getb (); (void) getb (); (void) getw ();
			(void) getw (); break;
		case 'b':
			(void) getb (); (void) getb (); (void) getw ();
			(void) getw (); (void) getw (); break;
		case 'c': {
			word n;
			(void) getb ();
			n = getb ();
			(void) getw (); (void) getw ();
			while (n-- > 0) { (void) getb (); (void) getw (); }
			break;
		}
		case 'R': (void) getw (); break;
		case 'r': break;
		case 'L': case 'J': {
			word n;
			(void) getw ();
			n = getw ();
			while (n-- > 0)
				(void) getw ();
			break;
		}
		case 'B': {
			word n, nf;
			(void) getw ();
			(void) getw ();
			n = getw ();
			while (n-- > 0)
				(void) getw ();
			(void) getw ();
			nf = getw ();
			while (nf-- > 0)
				(void) getw ();
			break;
		}
		default:
			errx (1, "c3: scan: bad stmt %c", t);
		}
	}
}

static void
scan ()
{
	byte t;

	rewind_input ();
	(void) getd ();			/* magic */
	while (1) {
		/* detect eof: read a byte; if we are at the end, stop */
		if (read (0, &t, 1) != 1)
			return;
		if (t == 'D') {
			byte fl;
			word sym;
			fl = getb ();
			sym = getw ();
			(void) getw ();	/* size */
			if ((fl & 3) != SC_EXTERN && ndefs < MAXDEF)
				defs[ndefs++] = sym;
			if (fl & 4) {
				byte c;
				while ((c = getb ()) != 0) {
					if (c == 1) {
						word n = getw ();
						while (n-- > 0)
							(void) getb ();
					} else {
						(void) getw ();
						(void) getw ();
					}
				}
			}
		} else if (t == 'F') {
			byte fl = getb ();
			word sym = getw ();
			if (!(fl & 4)) {
				(void) getb ();		/* 0xff */
				if ((fl & 3) != SC_EXTERN && ndefs < MAXDEF)
					defs[ndefs++] = sym;
				continue;
			}
			if (ndefs < MAXDEF)
				defs[ndefs++] = sym;
			skip_strings ();		/* args */
			skip_strings ();		/* vars */
			scan_code ();
			skip_strings ();		/* regs */
			(void) getb ();			/* end 0xff */
		} else {
			errx (1, "c3: scan: bad global %c", t);
		}
	}
}

static void
externs ()
{
	int i, j, k, isdef;

	for (i = 0; i < nrefs; ++i) {
		if (refs[i] & 0x8000)
			continue;
		isdef = 0;
		for (j = 0; j < ndefs; ++j)
			if (defs[j] == refs[i])
				isdef = 1;
		if (isdef)
			continue;
		for (k = 0; k < i; ++k)
			if (refs[k] == refs[i])
				break;
		if (k == i)
			printf ("extern %s\n", getsym (refs[i]));
	}
}

static void
strtable ()
{
	char	ch;
	int	n;

	if (lseek (strfd, 0L, SEEK_SET) < 0)
		err (1, "lseek");
	printf ("section .rodata\n");
	printf ("strtab:\n");
	while ((n = read (strfd, &ch, 1)) == 1)
		printf ("\tdb %u\n", (unsigned char) ch);
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

	printf ("[bits 16]\n[cpu 286]\n");
	scan ();
	externs ();
	strtable ();
	parse ();

	close (strfd);
	close (symfd);
	return 0;
}
