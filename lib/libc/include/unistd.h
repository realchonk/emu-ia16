#ifndef FILE_UNISTD_H
#define FILE_UNISTD_H
#include <stddef.h>
#include <cdefs.h>

__dead
void	 _exit ();

void	 close ();
int	 unlink ();
int	 write ();
int	 read ();
long	 lseek ();

int	 brk ();
void	*sbrk ();

#endif /* FILE_UNISTD_H */
