#include <_varargs.h>
#include <stdio.h>

extern int _vprintf ();

int
fprintf (file, fmt)
FILE		*file;
const char	*fmt;
{
	va_list	ap;
	int	n;

	va_start (ap, fmt);
	n = vfprintf (file, fmt, ap);
	va_end (ap);
	return n;
}
