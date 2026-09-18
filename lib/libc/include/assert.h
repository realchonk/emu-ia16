#ifndef FILE_ASSERT_H
#define FILE_ASSERT_H
#include <cdefs.h>

#define assert(expr) ((expr) ? 0 : __assert_fail (__FILE__, __LINE__, #expr))

__dead
void	__assert_fail ();

#endif /* FILE_ASSERT_H */
