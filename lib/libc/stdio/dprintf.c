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
dprintf (fd, fmt)
int		 fd;
const char	*fmt;
{
	va_list	ap;
	int	n;

	va_start (ap, fmt);
	n = _vprintf (dputc, &fd, fmt, ap);
	va_end (ap);
	return n;
}
