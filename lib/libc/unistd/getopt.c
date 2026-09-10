#include <unistd.h>
#include <string.h>
#include <stdio.h>
#include <err.h>

char	*optarg;
int	 optind, optopt, opterr = 1;

static char	**argv, *carg;
static int	  argc;

int
getopt (_argc, _argv, optstring)
int		  _argc;
char		**_argv;
const char	 *optstring;
{
	const char *s;

	if (optind == 0) {
		argv = _argv;
		argc = _argc;
		carg = NULL;
		optind = 1;
	}

	if (carg == NULL || *carg == '\0') {
		if (optind >= argc || argv[optind][0] != '-')
			return -1;
		if (strcmp (argv[optind], "--") == 0) {
			++optind;
			argc = 0;
			return -1;
		}
		carg = argv[optind++] + 1;
	}

	optarg = NULL;
	optopt = *carg++;
	s = strchr (optstring, optopt);
	if (s == NULL) {
		if (opterr)
			warnx ("invalid option: -%c", optopt);
		return '?';
	}

	if (s[1] != ':')
		return optopt;

	if (*carg != '\0') {
		optarg = carg;
		carg = NULL;
		return optopt;
	}

	if (optind >= argc) {
		if (opterr)
			warnx ("option requires argument: -%c", optopt);
		return '?';
	}

	optarg = argv[optind++];
	carg = NULL;

	return optopt;
}
