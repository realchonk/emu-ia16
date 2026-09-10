#include <_varargs.h>
#include <string.h>
#include <stdlib.h>
#include <stdio.h>
#include <errno.h>
#include <err.h>

__dead void
err (eval, fmt)
int		 eval;
const char	*fmt;
{
	va_list ap;

	va_start (ap, fmt);
	verr (eval, fmt, ap);
	va_end (ap);
}

void
warn (fmt)
const char *fmt;
{
	va_list ap;
	va_start (ap, fmt);
	vwarn (fmt, ap);
	va_end (ap);
}

__dead void
verr (eval, fmt, ap)
int		 eval;
const char	*fmt;
va_list		 ap;
{
	vwarn (fmt, ap);
	exit (eval);
}

void
vwarn (fmt, ap)
const char	*fmt;
va_list		 ap;
{
	fputs (getprogname (), stderr);
	if (fmt != NULL) {
		fputs (": ", stderr);
		vfprintf (stderr, fmt, ap);
	}
	fprintf (stderr, ": %s\n", strerror (errno));
}

