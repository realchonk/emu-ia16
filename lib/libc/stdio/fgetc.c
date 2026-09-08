#include <stdio.h>

int
fgetc (file)
FILE *file;
{
	unsigned char ch;
	if (file->peekc != EOF) {
		ch = file->peekc;
		file->peekc = EOF;
		return ch;
	}
	return fread (&ch, 1, 1, file) == 1 ? ch : EOF;
}

int
getc (file)
FILE *file;
{
	return fgetc (file);
}
