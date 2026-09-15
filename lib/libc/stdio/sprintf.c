#include <_varargs.h>
#include <stdio.h>

int
sprintf (s, fmt)
char		*s;
const char	*fmt;
{
	va_list	ap;
	int	n;

	va_start (ap, fmt);
	n = vsnprintf (s, -1, fmt, ap);
	va_end (ap);
	return n;
}
