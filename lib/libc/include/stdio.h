#ifndef FILE_STDIO_H
#define FILE_STDIO_H
#include <stddef.h>

#define EOF (-1)

typedef struct _FILE {
	void	 *cookie;
	int	  peekc;
	int	(*readfn)();
	int	(*writefn)();
	long	(*seekfn)();
	int	(*closefn)();
} FILE;

extern	FILE *__stdio_getstdx ();
#define stdin	__stdio_getstdx (0)
#define stdout	__stdio_getstdx (1)
#define stderr	__stdio_getstdx (2)

FILE	*funopen ();
FILE	*fdopen ();
FILE	*fopen ();
size_t	 fread ();
size_t	 fwrite ();
int	 fflush ();
int	 fclose ();

void	 setbuf ();

int	 getc ();
int	 getchar ();
int	 fgetc ();
int	 fputc ();
int	 fputs ();
int	 putc ();
int	 putchar ();
int	 puts ();
int	 ungetc ();

int	 printf ();
int	 dprintf ();
int	 fprintf ();
int	 vfprintf ();
int	 sprintf ();

#endif /* FILE_STDIO_H */
