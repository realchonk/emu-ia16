#include <stdlib.h>
#include <unistd.h>

void
exit (ec)
int ec;
{
	_exit (ec);
}
