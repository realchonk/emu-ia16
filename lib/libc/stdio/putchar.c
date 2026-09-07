#include <unistd.h>
#include <stdio.h>

int
putchar (ch)
int ch;
{
	return write (1, &ch, 1) == 1 ? (unsigned char)ch : EOF;
}
