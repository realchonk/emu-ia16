#include <string.h>

char *
strsep (string, delim)
char		**string;
const char	 *delim;
{
	char	*s, *n;

	s = *string;
	if (s == NULL)
		return NULL;

	n = s + strcspn (s, delim);
	*string = *n != '\0' ? n + 1 : NULL;
	*n = '\0';
	return s;
}
