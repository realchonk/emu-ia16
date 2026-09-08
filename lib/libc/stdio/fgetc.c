#include <stdio.h>

int
fgetc (file)
FILE *file;
{
	unsigned char ch;
	return fread (&ch, 1, 1, file) == 1 ? ch : EOF;
}

int
getc (file)
FILE *file;
{
	return fgetc (file);
}
