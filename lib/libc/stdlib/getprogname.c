#include <string.h>

extern const char *__progname;

const char *
getprogname ()
{
	const char *s;
	s = strrchr (__progname, '/');
	return s != NULL ? s + 1 : __progname;
}
