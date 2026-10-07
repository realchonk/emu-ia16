#include <unistd.h>
#include <stdlib.h>
#include <stdio.h>

struct buf {
	size_t size;
	char buf[];
};

struct buf *
balloc (size)
size_t size;
{
	struct buf *b;

	b = malloc (sizeof (struct buf) + size);
	b->size = size;

	return b;
}

void
bfree (b)
struct buf *b;
{
	free (b);
}

void
bfill (b, i)
struct buf	*b;
int		 i;
{
	memset (b->buf, i, b->size);
}

void
bwrite (fd, b)
int		 fd;
struct buf	*b;
{
	write (fd, b->buf, b->size);
	write (fd, "\n", 1);
}

int
main ()
{
	struct buf *a, *b;

	a = balloc (8);
	b = balloc (8);

	bfill (a, 'A');
	bfill (b, 'B');

	bwrite (1, a);
	bwrite (1, b);

	bfree (a);
	bfree (b);

	return 0;
}
