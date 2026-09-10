#ifndef FILE_STRING_H
#define FILE_STRING_H
#include <stddef.h>

int	 memcmp ();
void	*memset ();
void	*memcpy ();
void	*memmove ();

char	*strcpy ();
char	*strcat ();
char	*strchr ();
char	*strrchr ();
int	 strcmp ();
size_t	 strlen ();
size_t	 strnlen ();

char	*strerror ();

#endif /* FILE_STRING_H */
