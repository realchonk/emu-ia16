#ifndef FILE_STDIO_H
#define FILE_STDIO_H
#include <stddef.h>

#define EOF (-1)

typedef struct _FILE {
	void	 *cookie;
	int	(*readfn)();
	int	(*writefn)();
	long	(*seekfn)();
	int	(*closefn)();
} FILE;

extern FILE *stdin, *stdout, *stderr;

FILE	*funopen ();
FILE	*fdopen ();
size_t	 fread ();
size_t	 fwrite ();
int	 fflush ();
int	 fclose ();

int	 getc ();
int	 getchar ();
int	 fgetc ();
int	 fputc ();
int	 fputs ();
int	 putchar ();
int	 puts ();

int	 printf ();
int	 dprintf ();
int	 fprintf ();
int	 sprintf ();

#endif /* FILE_STDIO_H */
