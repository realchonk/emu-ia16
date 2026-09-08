#include <unistd.h>
#include <errno.h>

#define MINSTACK 512u
#define MAXDATA (65536u - MINSTACK)

extern char __brk[];
static void *__brkp = __brk;

int
brk (addr)
void *addr;
{
	if ((unsigned)addr > MAXDATA) {
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
