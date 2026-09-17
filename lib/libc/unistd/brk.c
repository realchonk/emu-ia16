#include <unistd.h>
#include <errno.h>

extern char __brk[], __brk_end[];
static void *__brkp = __brk;

int
brk (addr)
void *addr;
{
	if ((char *)addr > __brk_end) {
		errno = ENOMEM;
		return -1;
	}
	__brkp = addr;
	return 0;
}

void *
sbrk (inc)
int inc;
{
	void *obrk = __brkp;
	return brk ((char *)__brkp + inc) == 0 ? obrk : (void *)-1;
}
