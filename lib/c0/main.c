#include <_varargs.h>
#include <stdio.h>
#include "c0.h"

/*
 * main.c -- driver for c0, the first pass of the C compiler.  The
 * parser in y.tab.c (generated from c0.y by yacc) calls into decl.c,
 * expr.c, stmt.c and type.c to parse and type check a translation
 * unit; errors are reported here.
 */

FILE	*fstr, *fir;
int	 nerrors;

void
typerr (fmt)
const char *fmt;
{
	va_list ap;

	va_start (ap, fmt);
	fprintf (stderr, "%d: ", linenum);
	vfprintf (stderr, fmt, ap);
	fprintf (stderr, "\n");
	va_end (ap);
	++nerrors;
}

int
yyerror (msg)
char *msg;
{
	fprintf (stderr, "%d: %s\n", linenum, msg);
	++nerrors;
	return 0;
}

int
main (argc, argv)
int	  argc;
char	**argv;
{
	if (argc != 3) {
		fputs ("usage: c0 irfile strfile\n", stderr);
		return 1;
	}
	fir = fopen (argv[1], "w");
	if (fir == NULL) {
		fprintf (stderr, "cannot open IR file %s\n", argv[1]);
		return 1;
	}
	fstr = fopen (argv[2], "w");
	if (fstr == NULL) {
		fprintf (stderr, "cannot open string file %s\n", argv[2]);
		return 1;
	}

	yyparse ();
	if (nerrors != 0)
		fprintf (stderr, "c0: %d error%s\n", nerrors,
			 nerrors > 1 ? "s" : "");
	return nerrors != 0;
}
