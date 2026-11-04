/*
 * c3b -- allocated IR (CIR\003) -> MDIR (CIR\002).
 *
 * The second half of c3.  c3a has already decoded the SSA and computed the
 * register allocation, so this program only has to load the fixed-size
 * instruction image and emit machine-dependent IR.
 */
#include <string.h>
#include <unistd.h>
#include <fcntl.h>
#include <err.h>
#include "c3.h"

static void
copy_data ()
{
	byte flags, cnk;
	word n;

	flags = getb ();
	md_b ('D');
	md_b (flags);
	md_w (getw ());			/* sym */
	md_w (getw ());			/* size */
	if ((flags & 4) == 0)
		return;
	for (;;) {
		cnk = getb ();
		md_b (cnk);
		if (cnk == 0)
			return;
		switch (cnk) {
		case 0x01:
			n = getw ();
			md_w (n);
			while (n-- > 0)
				md_b (getb ());
			break;
		case 0x02:
		case 0x03:
			md_w (getw ());
			md_w (getw ());
			break;
		default:
			errx (1, "c3b: bad init chunk %#x", cnk);
		}
	}
}

static void
func ()
{
	byte flags;
	word sym, w;

	flags = getb ();
	sym = getw ();

	if ((flags & 4) == 0) {
		if (getb () != 0xff)
			errx (1, "c3b: malformed F");
		md_b ('F');
		md_b (flags);
		md_w (sym);
		md_b (0xff);
		return;
	}

	nloc = getw ();
	nargs = getw ();
	get (locsize, nloc * sizeof locsize[0]);

	nins = getw ();
	if (nins > MAXINS)
		errx (1, "c3b: too many instructions");
	get (ins, nins * (int) sizeof (struct insn));

	nargpool = getw ();
	if (nargpool > ARGPOOL)
		errx (1, "c3b: too many args");
	get (argpool, nargpool * sizeof argpool[0]);

	nval = getw ();
	if (nval > MAXVAL)
		errx (1, "c3b: too many values");
	get (vdw, VDWBYTES);

	framesize = (short) getw ();
	nsaves = getb ();
	get (saved, nsaves * sizeof saved[0]);
	nloc = (short) getw ();
	get (locsite, nloc * sizeof locsite[0]);
	w = getw ();
	if (w != nval)
		errx (1, "c3b: value-count mismatch");
	get (busybits, nins);
	get (vinfo, nval);
	get (vslot, nval * sizeof vslot[0]);
	get (vdir, nval);

	emit_func (sym, flags);
}

static void
parse ()
{
	byte type;

	if (lseek (0, 0L, SEEK_SET) != 0)
		err (1, "lseek");
	{
		char buf[4];
		get (buf, 4);
		if (memcmp (buf, "CIR\003", 4) != 0)
			errx (1, "c3b: invalid magic");
	}
	while (read (0, &type, 1) == 1) {
		switch (type) {
		case 'D':	copy_data (); break;
		case 'F':	func (); break;
		default:	errx (1, "c3b: invalid global %c", type);
		}
	}
}

static void
strtable ()
{
	byte	ch;
	long	n, i;

	n = lseek (strfd, 0L, SEEK_END);
	if (n < 0)
		err (1, "lseek");
	if (lseek (strfd, 0L, SEEK_SET) < 0)
		err (1, "lseek");
	if (n > 0xffff)
		errx (1, "c3b: string table too large");
	md_b ('T');
	md_w ((word) n);
	for (i = 0; i < n; ++i) {
		if (read (strfd, &ch, 1) != 1)
			err (1, "read()");
		md_b (ch);
	}
}

int
main (argc, argv)
int	 argc;
char	**argv;
{
	if (argc != 3)
		errx (1, "usage: /lib/c3b strfile symfile");

	strfd = open (argv[1], O_RDONLY);
	if (strfd < 0)
		err (1, "open(%s)", argv[1]);
	symfd = open (argv[2], O_RDONLY);
	if (symfd < 0)
		err (1, "open(%s)", argv[2]);

	md_b ('C'); md_b ('I'); md_b ('R'); md_b (2);
	strtable ();
	parse ();
	md_flush ();

	close (strfd);
	close (symfd);
	return 0;
}
