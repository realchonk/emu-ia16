#include <_varargs.h>
#include <stdio.h>

int
vprintf (fmt, ap)
const char	*fmt;
va_list		 ap;
{
	return vfprintf (stdout, fmt, ap);
}

