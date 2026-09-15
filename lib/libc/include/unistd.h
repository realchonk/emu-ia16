#ifndef FILE_UNISTD_H
#define FILE_UNISTD_H
#include <stddef.h>
#include <cdefs.h>

#define STDIN_FILENO	0
#define STDOUT_FILENO	1
#define STDERR_FILENO	2

#define SEEK_SET	0
#define SEEK_CUR	1
#define SEEK_END	2

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

int	 fork ();
int	 execv ();

#endif /* FILE_UNISTD_H */
