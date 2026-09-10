#ifndef FILE_UNISTD_H
#define FILE_UNISTD_H
#include <stddef.h>
#include <cdefs.h>

#define SEEK_SET 0
#define SEEK_CUR 1
#define SEEK_END

extern char	*optarg;
extern int	 optind, optopt, opterr;

__dead
void	 _exit ();

void	 close ();
int	 unlink ();
int	 write ();
int	 read ();
long	 lseek ();
int	 pread ();
int	 pwrite ();

int	 brk ();
void	*sbrk ();
int	 getopt ();

#endif /* FILE_UNISTD_H */
