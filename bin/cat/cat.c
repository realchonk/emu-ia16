#include <string.h>
#include <stdio.h>

static int
cat (file)
FILE *file;
{
	int ch;

	while ((ch = fgetc (file)) != EOF)
		putchar (ch);

	return 0;
}

int
main (argc, argv)
int	  argc;
char	**argv;
{
	FILE	*file;
	int	 i, r = 0;

	if (argc <= 1)
		return cat (stdin);

	for (i = 1; i < argc; ++i) {
		if (strcmp (argv[i], "-") == 0) {
			cat (stdin);
			continue;
		}

		file = fopen (argv[i], "r");
		if (file == NULL) {
			fprintf (stderr, "failed to open '%s'\n", argv[i]);
			r = 1;
			continue;
		}

		cat (file);

		fclose (file);
	}

	return r;
}
