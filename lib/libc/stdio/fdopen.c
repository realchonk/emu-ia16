#include <unistd.h>
#include <stdio.h>

static int
fd_writefn (cookie, ptr, num)
void		*cookie;
const void	*ptr;
size_t		 num;
{
	return write ((int)cookie, ptr, num);
}

static int
fd_readfn (cookie, ptr, num)
void	*cookie, *ptr;
size_t	 num;
{
	return read ((int)cookie, ptr, num);
}

static long
fd_seekfn (cookie, off, whence)
void	*cookie;
long	 off;
int	 whence;
{
	return lseek ((int)cookie, off, whence);
}

static int
fd_closefn (cookie)
void	*cookie;
{
	close ((int)cookie);
	return 0;
}

FILE *
fdopen (fd, mode)
int		 fd;
const char	*mode;
{
	(void)mode;
	return funopen ((void *)fd, fd_readfn, fd_writefn, fd_seekfn, fd_closefn);
}
