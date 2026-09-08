#include <_varargs.h>
#include <unistd.h>
#include <stdio.h>

extern int _vprintf ();

static void
sputc (p, c)
void	*p;
int	 c;
{
	char **s = p;
	**s = c;
	++*s;
}

int
sprintf (s, fmt)
char		*s;
const char	*fmt;
{
	va_list	ap;
	int	n;

	va_start (ap, fmt);
	n = _vprintf (sputc, &s, fmt, ap);
	va_end (ap);
	return n;
}
