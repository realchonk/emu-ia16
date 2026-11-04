/*
 * c3 I/O -- buffered byte stream output and little-endian input helpers.
 *
 * Both halves of c3 share this file: c3a reads SSA and writes CIR\003,
 * c3b reads CIR\003 and writes MDIR (CIR\002).
 */
#include <unistd.h>
#include <err.h>
#include "c3.h"

int	strfd, symfd;

/* ---- buffered output -------------------------------------------------- */
static byte	obuf[256];
static int	ocnt;

void
md_flush ()
{
	if (ocnt != 0) {
		if (write (1, obuf, ocnt) != ocnt)
			err (1, "write()");
		ocnt = 0;
	}
}

void
md_b (b)
int b;
{
	if (ocnt >= (int) sizeof obuf)
		md_flush ();
	obuf[ocnt++] = b;
}

void
md_w (w)
int w;
{
	md_b (w & 0xff);
	md_b ((w >> 8) & 0xff);
}

/* write a raw pool directly, bypassing the byte buffer */
void
raw (buf, num)
void	*buf;
int	 num;
{
	md_flush ();
	if (write (1, buf, num) != num)
		err (1, "write()");
}

/* ---- input helpers ---------------------------------------------------- */
void
get (buf, num)
void	*buf;
int	 num;
{
	int n;

	n = read (0, buf, num);
	if (n == num)
		return;
	if (n < 0)
		err (1, "read()");
	errx (1, "c3: eof");
}

byte
getb ()
{
	byte b;
	get (&b, 1);
	return b;
}

word
getw ()
{
	word w;
	get (&w, 2);
	return w;
}

dword
getd ()
{
	dword d;
	get (&d, 4);
	return d;
}
