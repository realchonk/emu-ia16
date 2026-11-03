#include <_varargs.h>
#include <string.h>
#include <stdlib.h>
#include <fcntl.h>
#include <unistd.h>
#include "c0.h"

/*
 * main.c -- driver for c0, the first pass of the C compiler.  The
 * parser in y.tab.c (generated from c0.y by yacc) calls into decl.c,
 * expr.c, stmt.c and type.c to parse and type check a translation
 * unit.  Everything here is kept small and free of stdio: the two
 * outputs are raw file descriptors written byte by byte, input is
 * read byte by byte, and diagnostics go through mini, a tiny
 * formatter (%s %d %c %o), so the stdio machinery stays out of this
 * 64K program.
 */

int	astfd, symfd, strfd;
int	nerrors;

/* ---------------- raw byte input and output ---------------- */

void
oputc (c, fd)
int c, fd;
{
	char ch = c;

	write (fd, &ch, 1);
}

void
oputs (s, fd)
char *s;
int fd;
{
	write (fd, s, (int) strlen (s));
}

static int	pback = -1;

int
c0getc ()
{
	char ch;

	if (pback >= 0) {
		ch = pback;
		pback = -1;
		return ch & 0xff;
	}
	if (read (0, &ch, 1) != 1)
		return -1;
	return ch & 0xff;
}

int
c0ungetc (c)
int c;
{
	pback = c;
	return c;
}

/* ---------------- diagnostics ---------------- */

/*
 * A tiny formatter for diagnostics (%s %d %c %o, at most two of
 * them).  Arguments are passed in fixed slots; they are ints, with
 * pointers fitting in one (both are 16 bits here).
 */
void
mini (fd, fmt, a1, a2)
int fd;
const char *fmt;
int a1, a2;
{
	char buf[8], *s;
	int c, i, a, n;

	while ((c = *fmt++) != '\0') {
		if (c != '%') {
			oputc (c, fd);
			continue;
		}
		c = *fmt++;
		a = a1;
		a1 = a2;
		switch (c) {
		case '\0':
			return;
		case 's':
			s = (char *) a;
			while (*s != '\0')
				oputc (*s++, fd);
			break;
		case 'd':
		case 'o':
			n = a;
			i = 0;
			if (n < 0 && c == 'd') {
				oputc ('-', fd);
				n = -n;
			}
			if (n == 0)
				buf[i++] = '0';
			while (n > 0) {
				if (c == 'd') {
					buf[i++] = '0' + n % 10;
					n /= 10;
				} else {
					buf[i++] = '0' + (n & 7);
					n >>= 3;
				}
			}
			while (i > 0)
				oputc (buf[--i], fd);
			break;
		case 'c':
			oputc (a, fd);
			break;
		default:
			oputc (c, fd);
			break;
		}
	}
}

void
typerr (fmt)
const char *fmt;
{
	va_list ap;
	int a1, a2;

	va_start (ap, fmt);
	a1 = va_arg (ap, int);
	a2 = va_arg (ap, int);
	va_end (ap);
	mini (2, "%d: ", linenum, 0);
	mini (2, fmt, a1, a2);
	oputc ('\n', 2);
	++nerrors;
}

int
yyerror (msg)
char *msg;
{
	mini (2, "%d: %s\n", linenum, (int) msg);
	++nerrors;
	return 0;
}

/* ---------------- driver ---------------- */

int
main (argc, argv)
int	  argc;
char	**argv;
{
	int	  i;

	i = 1;
	if (i < argc && strncmp (argv[i], "-mcmodel=", 9) == 0) {
		if (strcmp (argv[i] + 9, "large") == 0)
			farall = 1;
		else if (strcmp (argv[i] + 9, "small") == 0)
			farall = 0;
		else {
			mini (2, "c0: unknown code model: %s\n",
			      (int) (argv[i] + 9), 0);
			return 1;
		}
		++i;
	}
	if (argc - i != 3) {
		mini (2, "usage: c0 [-mcmodel=small|large] ast str sym\n",
		      0, 0);
		return 1;
	}
	astfd = open (argv[i], O_WRONLY | O_CREAT | O_TRUNC, 0644);
	strfd = open (argv[i + 1], O_RDWR | O_CREAT | O_TRUNC, 0644);
	symfd = open (argv[i + 2], O_WRONLY | O_CREAT | O_TRUNC, 0644);
	if (astfd < 0 || strfd < 0 || symfd < 0) {
		mini (2, "cannot open %s\n",
		      (int) (astfd < 0 ? argv[i]
			    : strfd < 0 ? argv[i + 1] : argv[i + 2]), 0);
		return 1;
	}

	oputc (0, strfd);		/* offset 0: the reserved pad byte */
	oputs ("CIR", astfd);
	oputc (0, astfd);

	yyparse ();

	close (astfd);
	close (strfd);
	close (symfd);
	if (nerrors != 0) {
		mini (2, "c0: %d errors\n", nerrors, 0);
		return 1;
	}
	return 0;
}
