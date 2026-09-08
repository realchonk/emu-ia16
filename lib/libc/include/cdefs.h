#ifndef FILE_CDEFS_H
#define FILE_CDEFS_H

#if __GNUC__
# define __dead __attribute__((noreturn))
#else
# define __dead
#endif

#endif /* FILE_CDEFS_H */
