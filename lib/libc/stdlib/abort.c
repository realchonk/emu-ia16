#include <unistd.h>
#include <stdlib.h>

__dead void
abort ()
{
	_exit (255);
}
