#include <unistd.h>
#include <stdlib.h>
#include <stdio.h>
#include <errno.h>
#include <err.h>

int
main (argc, argv)
int	  argc;
char	**argv;
{
	char	*string;
	int	 option, x;

	string = "Hello World";
	x = 0;

	while ((option = getopt (argc, argv, "e:s:x")) != -1) {
		switch (option) {
		case 'e':
			errno = atoi (optarg);
			err (1, "-e option specified");
		case 's':
			string = optarg;
			break;
		case 'x':
			++x;
			break;
		default:
			return 1;
		}
	}

	argv += optind;
	argc -= optind;

	printf ("%s (%d)\n", string, x);

	if (argc != 0) {
		for (x = 0; x < argc; ++x)
			printf ("[%d]: %s\n", x, argv[x]);
	}

	return 0;
}
