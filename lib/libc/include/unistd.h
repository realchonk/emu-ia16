#ifndef FILE_UNISTD_H
#define FILE_UNISTD_H
#include <stddef.h>

void	 _exit ();
int	 write ();
int	 read ();
long	 lseek ();
int	 brk ();
void	 close ();
void	*sbrk ();

#endif /* FILE_UNISTD_H */
