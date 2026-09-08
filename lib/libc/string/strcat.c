#include <string.h>

char *
strcat (d, s)
char		*d;
const char	*s;
{
	strcpy (d + strlen (d), s);
	return d;
}
