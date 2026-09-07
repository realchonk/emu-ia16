#include <stddef.h>
#include <unistd.h>

int
main ()
{
	const char s[] = "Hello World\n";
	write (1, s, sizeof (s) - 1);
	return 0;
}
