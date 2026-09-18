#include <unistd.h>
#include <fcntl.h>
#include <err.h>
#include "c1.h"

int	ofd = 1, tfd;
size_t	off;

void
put (buf, num)
const void	*buf;
int		 num;
{
	int n;

	n = write (ofd, buf, num);

	if (n == num) {
		return;
	} else if (n < 0) {
		err (1, "write()");
	} else {
		errx (1, "no space");
	}
}

void
putb (b)
byte b;
{
	put (&b, 1);
}

void
putw (w)
word w;
{
	put (&w, 2);
}

void
putd (d)
dword d;
{
	put (&d, 4);
}

void
get (buf, num)
void	*buf;
int	 num;
{
	int n;

	n = read (0, buf, num);

	if (n == num) {
		off += num;
	} else if (n < 0) {
		err (1, "read()");
	} else {
		errx (1, "eof");
	}

}

byte
getb ()
{
	byte b;
	return get (&b, 1), b;
}

word
getw ()
{
	word w;
	return get (&w, 2), w;
}

dword
getd ()
{
	dword d;
	return get (&d, 4), d;
}

int
main (argc, argv)
int	  argc;
char	**argv;
{
	if (argc != 2)
		errx (1, "usage: /lib/c1 tmpfile");

	tfd = open (argv[1], O_RDWR | O_CREAT, 0644);
	if (tfd < 0)
		err (1, "creat('%s')", argv[1]);

	if (write (ofd, "CIR\001", 4) != 4)
		err (1, "write()");

	parse ();

	close (tfd);
	return 0;
}
