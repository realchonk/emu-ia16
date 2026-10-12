#include <stdio.h>

char *
fgets (str, size, file)
char	*str;
int	 size;
FILE	*file;
{
	int n = 0, ch = EOF;

	do {
		if (n >= size - 1)
			break;

		ch = fgetc (file);
		if (ch == EOF)
			break;
		str[n++] = ch;
	} while (ch != '\n');
	str[n] = '\0';
	return ch != EOF || n > 0 ? str : NULL;
}
