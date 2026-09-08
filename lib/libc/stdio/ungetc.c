#include <stdio.h>

int
ungetc (c, file)
int	 c;
FILE	*file;
{
	if (file->peekc != EOF)
		return EOF;
	file->peekc = c;
	return c;
}
