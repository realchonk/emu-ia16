#ifndef FILE_STRING_H
#define FILE_STRING_H
#include <stddef.h>

void	*memset (void *, int, size_t);
void	*memcpy (void *restrict, const void *restrict, size_t);
void	*memmove (void *, const void *, size_t);
size_t	 strlen (const char *);

#endif /* FILE_STRING_H */
