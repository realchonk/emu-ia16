#include <_varargs.h>
#include <stdio.h>

extern int _vprintf ();

static void
_fputc (p, c)
void	*p;
int	 c;
{
	fputc (c, p);
}

int
fprintf (file, fmt)
FILE		*file;
const char	*fmt;
{
	va_list	ap;
	int	n;

	va_start (ap, fmt);
	n = _vprintf (_fputc, file, fmt, ap);
	va_end (ap);
	return n;
}
