#ifndef FILE_STRING_H
#define FILE_STRING_H
#include <stddef.h>

int	 memcmp ();
void	*memset ();
void	*memcpy ();
void	*memmove ();

char	*strdup ();
char	*strndup ();
char	*strcpy ();
char	*strncpy ();
char	*strcat ();
char	*strncat ();
char	*strchr ();
char	*strrchr ();
char	*strsep ();
char	*strpbrk ();
int	 strcmp ();
int	 strncmp ();
size_t	 strlen ();
size_t	 strnlen ();
size_t	 strspn ();
size_t	 strcspn ();

char	*strerror ();

#endif /* FILE_STRING_H */
