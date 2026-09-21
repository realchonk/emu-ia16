#ifndef FILE_STDLIB_H
#define FILE_STDLIB_H
#include <stddef.h>
#include <cdefs.h>

__dead
void	 exit ();
__dead
void	 abort ();

void	*malloc ();
void	*calloc ();
void	*realloc ();
void	 free ();
void	 freeall ();

int	 atoi ();

const char *getprogname ();

#endif /* FILE_STDLIB_H */
