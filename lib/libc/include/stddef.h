#ifndef FILE_STDDEF_H
#define FILE_STDDEF_H

typedef unsigned	size_t;
typedef int		ptrdiff_t;

#define NULL	0
#define offsetof(t, m)	((size_t) &((t *) 0)->m)

#endif /* FILE_STDDEF_H */
