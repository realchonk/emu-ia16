#include <_varargs.h>
#include <stdio.h>

extern int _vprintf ();

static void
_putc (p, c)
void	*p;
int	 c;
{
	(void)p;
	putchar (c);
}

int
printf (fmt)
const char *fmt;
{
	va_list	ap;
	int	n;

	va_start (ap, fmt);
	n = _vprintf (_putc, NULL, fmt, ap);
	va_end (ap);
	return n;
}

