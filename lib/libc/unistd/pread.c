#include <unistd.h>

int
pread (fd, buf, num, off)
int	 fd;
char	*buf;
size_t	 num;
long	 off;
{
	long old;
	int n = -1;

	old = lseek (fd, 0L, SEEK_CUR);
	if (old == -1)
		return -1;

	if (lseek (fd, off, SEEK_SET) == -1)
		goto skip;

	n = read (fd, buf, num);

skip:
	if (lseek (fd, old, SEEK_SET) == -1)
		return -1;
	
	return n;
}
