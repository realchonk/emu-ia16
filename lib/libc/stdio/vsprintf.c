#include <_varargs.h>
#include <stdio.h>

int
vsprintf (s, fmt, ap)
char		*s;
const char	*fmt;
va_list		 ap;
{
	return vsnprintf (s, -1, fmt, ap);
}
