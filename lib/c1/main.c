#include <string.h>
#include <stdio.h>
#include <err.h>

FILE *irfile, *symfile;


int
main (argc, argv)
int	  argc;
char	**argv;
{
	char buf[4];

	if (argc != 3) {
		fputs ("usage: c1 irfile symfile\n", stderr);
		return 1;
	}

	irfile = fopen (argv[1], "r");
	if (irfile == NULL)
		err (1, "fopen('%s')", argv[1]);

	symfile = fopen (argv[2], "r");
	if (irfile == NULL)
		err (1, "fopen('%s')", argv[2]);

	if (fread (buf, 1, 4, irfile) != 4)
		err (1, "fread()");

	if (memcmp (buf, "CIR", 4) != 0)
		errx (1, "invalid magic");
}
