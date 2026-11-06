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

#ifndef BINDIR
# define BINDIR PREFIX	"/bin"
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

#ifndef C2
# define C2		LIBDIR "/c2"
#endif

#ifndef C3A
# define C3A		LIBDIR "/c3a"
#endif

#ifndef C3B
# define C3B		LIBDIR "/c3b"
#endif

#ifndef C4
# define C4		LIBDIR "/c4"
#endif

#ifndef LD
# define LD		BINDIR "/ld"
#endif

#ifndef CRT0
# define CRT0		LIBDIR "/crt0.o"
#endif

#ifndef LIBC
# define LIBC		LIBDIR "/libc/libc.a"
#endif

#ifndef AS
# define AS		BINDIR "/as"
#endif

static bool keeptemps = false;
static const char *cmodel = NULL;	/* NULL means -mcmodel=small */

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
runc0 (input, astfile, strfile, symfile, model)
char *input, *astfile, *strfile, *symfile;
const char *model;
{
	char	*argv[6];
	int	 n, ws;

	n = 0;
	argv[n++] = C0;
	if (model != NULL)
		argv[n++] = (char *) model;
	argv[n++] = astfile;
	argv[n++] = strfile;
	argv[n++] = symfile;
	argv[n] = NULL;

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
runc1 (astfile, irfile, tmpfile)
char *astfile, *irfile, *tmpfile;
{
	char	*argv[3];
	int	 ws;

	argv[0] = C1;
	argv[1] = tmpfile;
	argv[2] = NULL;

	switch (fork ()) {
	case -1:
		err (1, "c1: fork()");
	case 0:
		close (0);
		if (open (astfile, O_RDONLY) != 0)
			err (1, "c1: open('%s')", astfile);
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
runc2 (ssafile, optfile)
char *ssafile, *optfile;
{
	char	*argv[2];
	int	 ws;

	argv[0] = C2;
	argv[1] = NULL;

	switch (fork ()) {
	case -1:
		err (1, "c2: fork()");
	case 0:
		close (0);
		if (open (ssafile, O_RDONLY) != 0)
			err (1, "c2: open('%s')", ssafile);
		close (1);
		if (open (optfile, O_WRONLY | O_CREAT | O_TRUNC, 0644) != 1)
			err (1, "c2: open('%s')", optfile);
		execv (C2, argv);
		err (1, "c2: execv('%s')", C2);
	default:
		if (wait (&ws) == -1)
			err (1, "c2: wait()");
		if (!WIFEXITED (ws) || WEXITSTATUS (ws) != 0) {
			warnx ("c2");
			return 1;
		}
		return 0;
	}
}

static int
runc3a (ssafile, allocfile)
char *ssafile, *allocfile;
{
	char	*argv[2];
	int	 ws;

	argv[0] = C3A;
	argv[1] = NULL;

	switch (fork ()) {
	case -1:
		err (1, "c3a: fork()");
	case 0:
		close (0);
		if (open (ssafile, O_RDONLY) != 0)
			err (1, "c3a: open('%s')", ssafile);
		close (1);
		if (open (allocfile, O_WRONLY | O_CREAT | O_TRUNC, 0644) != 1)
			err (1, "c3a: open('%s')", allocfile);
		execv (C3A, argv);
		err (1, "c3a: execv('%s')", C3A);
	default:
		if (wait (&ws) == -1)
			err (1, "c3a: wait()");
		if (!WIFEXITED (ws) || WEXITSTATUS (ws) != 0) {
			warnx ("c3a");
			return 1;
		}
		return 0;
	}
}

static int
runc3b (allocfile, asfile, strfile, symfile)
char *allocfile, *asfile, *strfile, *symfile;
{
	char	*argv[5];
	int	 ws;

	argv[0] = C3B;
	argv[1] = strfile;
	argv[2] = symfile;
	argv[3] = NULL;
	argv[4] = NULL;

	switch (fork ()) {
	case -1:
		err (1, "c3b: fork()");
	case 0:
		close (0);
		if (open (allocfile, O_RDONLY) != 0)
			err (1, "c3b: open('%s')", allocfile);
		close (1);
		if (open (asfile, O_WRONLY | O_CREAT | O_TRUNC, 0644) != 1)
			err (1, "c3b: open('%s')", asfile);
		execv (C3B, argv);
		err (1, "c3b: execv('%s')", C3B);
	default:
		if (wait (&ws) == -1)
			err (1, "c3b: wait()");
		if (!WIFEXITED (ws) || WEXITSTATUS (ws) != 0) {
			warnx ("c3b");
			return 1;
		}
		return 0;
	}
}

static int
runc4 (mdifile, symfile, objfile)
char *mdifile, *symfile, *objfile;
{
	char	*argv[5];
	int	 ws;

	argv[0] = C4;
	argv[1] = "-o";
	argv[2] = objfile;
	argv[3] = symfile;
	argv[4] = NULL;

	switch (fork ()) {
	case -1:
		err (1, "c4: fork()");
	case 0:
		close (0);
		if (open (mdifile, O_RDONLY) != 0)
			err (1, "c4: open('%s')", mdifile);
		execv (C4, argv);
		err (1, "c4: execv('%s')", C4);
	default:
		if (wait (&ws) == -1)
			err (1, "c4: wait()");
		if (!WIFEXITED (ws) || WEXITSTATUS (ws) != 0) {
			warnx ("c4");
			return 1;
		}
		return 0;
	}
}

static int
slen (s)
const char *s;
{
	const char *p;

	p = s;
	while (*p)
		++p;
	return p - s;
}

static int
scopy (d, s)
char *d;
const char *s;
{
	char *p;

	p = d;
	while ((*p++ = *s++) != 0)
		;
	return p - d - 1;
}

static int
runld (objs, nobj, largs, nlargs, out, stack)
char **objs;
int nobj;
char **largs;
int nlargs;
const char *out;
int stack;
{
	char	*argv[96];
	char	 sbuf[32];
	char	 stackbuf[32];
	int	 n, ws;

	if (nobj + nlargs + 8 >= (int) (sizeof argv / sizeof argv[0]))
		errx (1, "cc: too many link inputs");
	n = 0;
	argv[n++] = LD;
	argv[n++] = "-o";
	argv[n++] = (char *) out;
	scopy (stackbuf, "-mstack=");
	scopy (stackbuf + slen (stackbuf), sbuf);
	/* print the stack size without stdio */
	if (stack < 0)
		stack = 0;
	{
		char tmp[12];
		int i, v;
		i = 0;
		v = stack;
		if (v == 0)
			tmp[i++] = '0';
		while (v > 0) {
			tmp[i++] = '0' + v % 10;
			v /= 10;
		}
		{
			int j;
			for (j = 0; j < i; ++j)
				sbuf[j] = tmp[i - 1 - j];
			sbuf[i] = 0;
		}
	}
	scopy (stackbuf, "-mstack=");
	scopy (stackbuf + slen (stackbuf), sbuf);
	argv[n++] = stackbuf;
	argv[n++] = CRT0;
	{
		int i;
		for (i = 0; i < nobj; ++i)
			argv[n++] = objs[i];
	}
	{
		int i;
		for (i = 0; i < nlargs; ++i)
			argv[n++] = largs[i];
	}
	argv[n++] = LIBC;
	argv[n] = NULL;

	switch (fork ()) {
	case -1:
		err (1, "ld: fork()");
	case 0:
		execv (LD, argv);
		err (1, "ld: execv('%s')", LD);
	default:
		if (wait (&ws) == -1)
			err (1, "ld: wait()");
		if (!WIFEXITED (ws) || WEXITSTATUS (ws) != 0) {
			warnx ("ld");
			return 1;
		}
		return 0;
	}
}

static int
assemble (input, output)
const char *input;
const char *output;
{
	char	*argv[8];
	int	 ws;

	argv[0] = AS;
	argv[1] = "-felf32";
	argv[2] = "-mcpu=80286";
	argv[3] = "-o";
	argv[4] = (char *) output;
	argv[5] = (char *) input;
	argv[6] = NULL;

	switch (fork ()) {
	case -1:
		err (1, "as: fork()");
	case 0:
		execv (AS, argv);
		err (1, "as: execv('%s')", AS);
	default:
		if (wait (&ws) == -1)
			err (1, "as: wait()");
		if (!WIFEXITED (ws) || WEXITSTATUS (ws) != 0) {
			warnx ("as");
			return 1;
		}
		return 0;
	}
}

static int
isasm (name)
const char *name;
{
	int n;

	n = strlen (name);
	if (n > 2 && name[n - 2] == '.'
	    && (name[n - 1] == 's' || name[n - 1] == 'S'))
		return 1;
	if (n > 4 && strcmp (name + n - 4, ".asm") == 0)
		return 1;
	return 0;
}

static int
compile (arg)
char **arg;
{
	char	*i, *ast, *str, *sym, *tmp, *ssa, *opt, *alc, *mdi, *obj;
	int	 ret = -1;

	i	= ssuffix (*arg, "i");
	ast	= ssuffix (*arg, "ast");
	str	= ssuffix (*arg, "str");
	sym	= ssuffix (*arg, "sym");
	tmp	= ssuffix (*arg, "tmp");
	ssa	= ssuffix (*arg, "ssa");
	opt	= ssuffix (*arg, "opt");
	alc	= ssuffix (*arg, "alc");
	mdi	= ssuffix (*arg, "mdi");
	obj	= ssuffix (*arg, "o");

	printf ("%s:\n", *arg);

	if (runcpp (*arg, i) != 0)
		goto fail;

	if (runc0 (i, ast, str, sym, cmodel) != 0)
		goto fail;

	if (runc1 (ast, ssa, tmp) != 0)
		goto fail;

	if (runc2 (ssa, opt) != 0)
		goto fail;

	if (runc3a (opt, alc) != 0)
		goto fail;

	if (runc3b (alc, mdi, str, sym) != 0)
		goto fail;

	if (runc4 (mdi, sym, obj) != 0)
		goto fail;

	ret = 0;

fail:
	if (!keeptemps) {
		unlink (i);
		unlink (sym);
		unlink (str);
		unlink (ast);
		unlink (tmp);
		unlink (opt);
		unlink (alc);
		unlink (mdi);
	}
	free (i);
	free (sym);
	free (str);
	free (ast);
	free (tmp);
	free (opt);
	free (alc);
	free (mdi);
	free (obj);
	return ret;
}

int
main (argc, argv)
int	  argc;
char	**argv;
{
	int	 i, n;
	char	*out, *objs[64], *largs[32];
	int	 nobjs, nlargs, stack;
	bool	 compileonly;

	keeptemps = false;
	compileonly = false;
	out = NULL;
	nobjs = 0;
	nlargs = 0;
	stack = 2048;

	for (i = 1; i < argc; ) {
		if (strcmp (argv[i], "-k") == 0) {
			keeptemps = true;
		} else if (strcmp (argv[i], "-c") == 0) {
			compileonly = true;
		} else if (strncmp (argv[i], "-mcmodel=", 9) == 0) {
			if (strcmp (argv[i] + 9, "large") == 0)
				cmodel = "-mcmodel=large";
			else if (strcmp (argv[i] + 9, "small") == 0)
				cmodel = NULL;
			else
				errx (1, "unknown code model: %s",
				      argv[i] + 9);
		} else if (strcmp (argv[i], "-o") == 0 && i + 1 < argc) {
			out = argv[++i];
		} else if (strncmp (argv[i], "-o", 2) == 0) {
			out = argv[i] + 2;
		} else if (strncmp (argv[i], "-mstack=", 8) == 0) {
			stack = atoi (argv[i] + 8);
		} else if (strncmp (argv[i], "-L", 2) == 0
			   || strncmp (argv[i], "-l", 2) == 0) {
			if (nlargs < 32)
				largs[nlargs++] = argv[i];
		} else {
			++i;
			continue;
		}

		memmove (argv + i, argv + i + 1,
			 sizeof (char *) * (argc - i));
		--argc;
	}

	for (i = 1; i < argc; ++i) {
		if (argv[i] == NULL || argv[i][0] == '-')
			continue;
		n = slen (argv[i]);
		if (!compileonly && n > 2 && argv[i][n - 2] == '.'
		    && argv[i][n - 1] == 'o') {
			if (nobjs < 64)
				objs[nobjs++] = argv[i];
			continue;
		}
		if (!compileonly && n > 2 && argv[i][n - 2] == '.'
		    && argv[i][n - 1] == 'a') {
			if (nobjs < 64)
				objs[nobjs++] = argv[i];
			continue;
		}
		if (isasm (argv[i])) {
			char *aout;

			aout = ssuffix (argv[i], "o");
			if (assemble (argv[i], aout) != 0) {
				free (aout);
				return 1;
			}
			if (!compileonly) {
				if (nobjs < 64)
					objs[nobjs++] = aout;
				else
					free (aout);
			} else
				free (aout);
			continue;
		}
		if (compile (&argv[i]) != 0)
			return 1;
		if (!compileonly) {
			if (nobjs < 64)
				objs[nobjs++] = ssuffix (argv[i], "o");
		}
	}

	if (!compileonly) {
		if (out == NULL)
			out = "a.out";
		if (runld (objs, nobjs, largs, nlargs, out, stack) != 0)
			return 1;
	}
	return 0;
}

