#include <stdio.h>

FILE *stdin, *stdout, *stderr;

void
__init_stdio ()
{
	stdin = fdopen (0, "r");
	stdout = fdopen (1, "w");
	stderr = fdopen (2, "w");
}
