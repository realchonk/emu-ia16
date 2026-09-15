#include <_varargs.h>
#include <unistd.h>
#include <stdio.h>

extern int _vprintf ();

static void
dputc (p, c)
void	*p;
int	 c;
{
	write (*(int *)p, &c, 1);
}

int
vdprintf (fd, fmt, ap)
int		 fd;
const char	*fmt;
va_list		 ap;
{
	return _vprintf (dputc, &fd, fmt, ap);
}
