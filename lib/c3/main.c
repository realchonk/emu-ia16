#include <_varargs.h>
#include <assert.h>
#include <unistd.h>
#include <stdlib.h>
#include <string.h>
#include <fcntl.h>
#include <stdio.h>
#include <err.h>
#include "c3.h"

size_t	off;
int	strfd, symfd;

void
putb (b)
byte b;
{
	printf ("\tdb %u\n", (unsigned)b);
}

void
putw (w)
word w;
{
	printf ("\tdw %u\n", (unsigned)w);
}

void
putd (d)
dword d;
{
	printf ("\tdd %lu\n", (unsigned long)d);
}

void
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

byte
getb ()
{
	byte b;
	return get (&b, 1), b;
}

word
getw ()
{
	word w;
	return get (&w, 2), w;
}

dword
getd ()
{
	dword d;
	return get (&d, 4), d;
}

const char *
getsym (sym)
word sym;
{
	static char buf[64];
	if (sym & 0x8000) {
		snprintf (buf, sizeof (buf), ".L%u", (unsigned)(sym & 0x7fff));
	} else {
		pread (symfd, buf, sizeof (buf) - 1, (long)sym);
		buf[strnlen (buf, sizeof (buf) - 1)] = '\0';
	}
	return buf;
}

void
section (name)
const char	*name;
{
	static const char *current = NULL;

	if (current != NULL && strcmp (name, current) == 0)
		return;

	printf ("section %s\n", name);
}

void
comment (fmt)
const char *fmt;
{
	va_list ap;

	va_start (ap, fmt);
	printf ("; ");
	vprintf (fmt, ap);
	printf ("\n");
	va_end (ap);
}

void
extrn (sym)
word sym;
{
	printf ("extern %s\n", getsym (sym));
}

void
globl (sym)
word sym;
{
	printf ("global %s\n", getsym (sym));
}

void
resb (size)
word size;
{
	printf ("\tresb %u\n", (unsigned)size);
}

void
label (sym)
word sym;
{
	printf ("%s:\n", getsym (sym));
}

static void
def ()
{
	byte	flags, cnk;
	word	sym, sym2, size, i, j, n;
	int	sc;

	flags	= getb ();
	sym	= getw ();
	size	= getw ();

	sc = flags & 3;

	if (sc == SC_EXTERN) {
		assert ((flags & 4) == 0);
		extrn (sym);
		return;
	}

	section (flags & 4 ? ".data" : ".bss");

	if (sc == SC_GLOBAL)
		globl (sym);

	label (sym);

	if ((flags & 4) == 0) {
		resb (size);
		return;
	}

	for (i = 0; (cnk = getb ()) != 0x00; ) {
		switch (cnk) {
		case 0x01:
			n = getw ();
			for (j = 0; j < n; ++i, ++j)
				putb (getb ());
			break;
		case 0x02:
			sym2 = getw ();
			n = getw ();
			printf ("dw %s + %u\n", getsym (sym2), (unsigned)n);
			break;
		case 0x03:
			sym2 = getw ();
			n = getw ();
			n = getw ("dw .strtab + %u + %u\n", (unsigned)sym2, (unsigned)n);
			break;
		default:
			abort ();
		}
	}
}

static void
func ()
{
	byte	flags;
	word	sym;
	int	sc;

	flags	= getb ();
	sym	= getw ();

	sc = flags & 3;
	if ((flags & 4) == 0) {

	}
}

void
parse ()
{
	char	buf[4];
	byte	type;

	if (read (0, buf, 4) != 4)
		err (1, "fread()");

	if (memcmp (buf, "CIR\001", 4) != 0)
		errx (1, "invalid magic");

	off = 4;

	while (1) {
		switch (read (0, &type, 1)) {
		case 0:
			return;
		case 1:
			break;
		default:
			err (1, "read()");
		}
		++off;

		switch (type) {
		case 'D':
			def ();
			break;
		case 'F':
			func ();
			break;
		default:
			errx (1, "0x%zx: invalid global: %c (%x)", off - 1, type, type);
		}
	}
}

static void
strtab ()
{
	section (".rodata.str");
	comment ("TODO");
	
}

int
main (argc, argv)
int	  argc;
char	**argv;
{
	if (argc != 3)
		errx (1, "usage: /lib/c3 strfile symfile");

	strfd = open (argv[1], O_RDONLY);
	if (strfd < 0)
		err (1, "open('%s')", argv[1]);

	symfd = open (argv[2], O_RDONLY);
	if (symfd < 0)
		err (1, "open('%s')", argv[2]);

	puts ("[bits 16]");
	puts ("[cpu 286]");

	parse ();
	strtab ();

	close (strfd);
	close (symfd);
	return 0;
}
