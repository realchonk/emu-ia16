#include <stddef.h>
#include <string.h>
#include <unistd.h>

int main (int argc, char **argv)
{
	int i;

	for (i = 1; i < argc; i++) {
		if (i != 1)
			write (1, " ", 1);
		write (1, argv[i], strlen (argv[i]));
	}
	write (1, "\n", 1);
	return (0);
}
