#ifndef FILE_STDLIB_H
#define FILE_STDLIB_H
#include <stddef.h>
#include <cdefs.h>

__dead
void	 exit ();

void	*malloc ();
void	*calloc ();
void	*realloc ();
void	 free ();

int	 atoi ();

#endif /* FILE_STDLIB_H */
