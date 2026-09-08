#include <stdio.h>

static FILE *_stdin, *_stdout, *_stderr;

FILE *
__stdio_getstdx (idx)
int idx;
{
	switch (idx) {
	case 0:
		if (_stdin == NULL)
			_stdin = fdopen (0, "r");
		return _stdin;
	case 1:
		if (_stdout == NULL)
			_stdout = fdopen (1, "w");
		return _stdout;
	case 2:
		if (_stderr == NULL)
			_stderr = fdopen (2, "w");
		return _stderr;
	default:
		__builtin_unreachable ();
	}
}
