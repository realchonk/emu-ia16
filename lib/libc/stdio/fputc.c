#include <stdio.h>

int
fputc (ch, file)
FILE	*file;
int	 ch;
{
	return fwrite (&ch, 1, 1, file) == 1 ? ch : EOF;
}

int
putc (ch, file)
FILE	*file;
int	 ch;
{
	return fputc (ch, file);
}
