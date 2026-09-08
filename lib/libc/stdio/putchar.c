#include <unistd.h>
#include <stdio.h>

int
putchar (ch)
int ch;
{
	return fwrite (&ch, 1, 1, stdout) == 1 ? (unsigned char)ch : EOF;
}
