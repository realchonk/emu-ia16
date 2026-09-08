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
vfprintf (file, fmt, ap)
FILE		*file;
const char	*fmt;
va_list		 ap;
{
	return _vprintf (_fputc, file, fmt, ap);
}
