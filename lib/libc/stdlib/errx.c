#include <_varargs.h>
#include <string.h>
#include <stdlib.h>
#include <stdio.h>
#include <errno.h>
#include <err.h>

__dead void
errx (eval, fmt)
int		 eval;
const char	*fmt;
{
	va_list ap;
	va_start (ap, fmt);
	verrx (eval, fmt, ap);
	va_end (ap);
}

void
warnx (fmt)
const char *fmt;
{
	va_list ap;
	va_start (ap, fmt);
	vwarnx (fmt, ap);
	va_end (ap);
}

__dead void
verrx (eval, fmt, ap)
int		 eval;
const char	*fmt;
va_list		 ap;
{
	vwarnx (fmt, ap);
	exit (eval);
}

void
vwarnx (fmt, ap)
const char	*fmt;
va_list		 ap;
{
	fputs (getprogname (), stderr);
	if (fmt != NULL) {
		fputs (": ", stderr);
		vfprintf (stderr, fmt, ap);
	}
	fputc ('\n', stderr);
}


