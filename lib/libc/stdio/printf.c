#include <_varargs.h>
#include <stdio.h>

int
printf (fmt)
const char *fmt;
{
	va_list	ap;
	int	n;

	va_start (ap, fmt);
	n = vprintf (fmt, ap);
	va_end (ap);
	return n;
}

