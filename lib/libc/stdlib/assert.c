#include <assert.h>
#include <stdlib.h>
#include <stdio.h>

__dead void
__assert_fail (file, line, s)
const char	*file, *s;
int		 line;
{
	fprintf (stderr, "%s: %s:%d: Assertion `%s` failed.\n",
	         getprogname (), file, line, s);
	abort ();
}
