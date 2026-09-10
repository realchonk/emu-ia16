#include <sys/wait.h>
#include <stdbool.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <fcntl.h>
#include <stdio.h>
#include <err.h>

#ifndef PREFIX
# define PREFIX		""
#endif

#ifndef LIBDIR
# define LIBDIR PREFIX	"/lib"
#endif

#ifndef INCDIR
# define INCDIR PREFIX	"/include"
#endif

#ifndef CPP
# define CPP		LIBDIR "/cpp"
#endif

#ifndef C0
# define C0		LIBDIR "/c0"
#endif

#ifndef C1
# define C1		LIBDIR "/c1"
#endif

static bool keeptemps = false;

static char *
ssuffix (name, sufx)
const char *name, *sufx;
{
	const char	*p;
	char		*s;
	size_t		 ln, ls;

	ls = strlen (sufx);
	p = strrchr (name, '.');
	ln = p != NULL ? (size_t)(p - name) : strlen (name);
	s = malloc (ln + ls + 2);
	if (s == NULL)
		err (1, "malloc()");

	memcpy (s, name, ln);
	s[ln] = '.';
	memcpy (s + ln + 1, sufx, ls + 1);
	return s;
}

static int
runcpp (input, output)
char *input, *output;
{
	char	*argv[5];
	int	 ws;

	argv[0] = CPP;
	argv[1] = "-I" INCDIR;
	argv[2] = input;
	argv[3] = output;
	argv[4] = NULL;

	switch (fork ()) {
	case -1:
		err (1, "cpp: fork()");
	case 0:
		execv (CPP, argv);
		err (1, "cpp: execv('%s')", CPP);
	default:
		if (wait (&ws) == -1)
			err (1, "cpp: wait()");
		if (!WIFEXITED (ws) || WEXITSTATUS (ws) != 0) {
			warnx ("cpp");
			return 1;
		}
		return 0;
	}
}

static int
runc0 (input, astfile, strfile, symfile)
char *input, *astfile, *strfile, *symfile;
{
	char	*argv[5];
	int	 ws;

	argv[0] = C0;
	argv[1] = astfile;
	argv[2] = strfile;
	argv[3] = symfile;
	argv[4] = NULL;

	switch (fork ()) {
	case -1:
		err (1, "c0: fork()");
	case 0:
		close (0);
		if (open (input, O_RDONLY) != 0)
			err (1, "c0: open('%s')", input);
		execv (C0, argv);
		err (1, "c0: execv('%s')", C0);

	default:
		if (wait (&ws) == -1)
			err (1, "c0: wait()");
		if (!WIFEXITED (ws) || WEXITSTATUS (ws) != 0) {
			warnx ("c0");
			return 1;
		}
		return 0;
	}
}

static int
runc1 (astfile, symfile, irfile)
char *astfile, *symfile, *irfile;
{
	char	*argv[4];
	int	 ws;

	argv[0] = C1;
	argv[1] = astfile;
	argv[2] = symfile;
	argv[3] = NULL;

	switch (fork ()) {
	case -1:
		err (1, "10: fork()");
	case 0:
		close (1);
		if (open (irfile, O_WRONLY | O_CREAT | O_TRUNC, 0644) != 1)
			err (1, "c1: open('%s')", irfile);
		execv (C1, argv);
		err (1, "c1: execv('%s')", C1);

	default:
		if (wait (&ws) == -1)
			err (1, "c1: wait()");
		if (!WIFEXITED (ws) || WEXITSTATUS (ws) != 0) {
			warnx ("c1");
			return 1;
		}
		return 0;
	}
}

static int
compile (arg)
char **arg;
{
	char	*i, *ast, *str, *sym, *ir;
	int	 ret = -1;

	i	= ssuffix (*arg, "i");
	ast	= ssuffix (*arg, "ast");
	str	= ssuffix (*arg, "str");
	sym	= ssuffix (*arg, "sym");
	ir	= ssuffix (*arg, "ir");

	printf ("%s:\n", *arg);

	if (runcpp (*arg, i) != 0)
		goto fail;

	if (runc0 (i, ast, str, sym) != 0)
		goto fail;

	if (runc1 (ast, sym, ir) != 0)
		goto fail;

	ret = 0;

fail:
	if (!keeptemps) {
		unlink (i);
		unlink (ast);
		unlink (sym);
		unlink (str);
		unlink (ir);
	}
	free (i);
	free (ast);
	free (sym);
	free (str);
	free (ir);
	return ret;
}

int
main (argc, argv)
int	  argc;
char	**argv;
{
	int	 i;

	/* parse options cc understands */
	for (i = 1; i < argc; ++i) {
		if (strcmp (argv[i], "-k") == 0) {
			keeptemps = true;
		} else {
			continue;
		}

		memmove (argv, argv + 1, sizeof (char *) * (argc - i));
		--argc;
	}

	/* compile files */
	for (i = 1; i < argc; ++i) {
		if (argv[i] != NULL && argv[i][0] != '-') {
			if (compile (&argv[i]) != 0)
				return 1;
		}
	}

	return 0;
}
