/*
 * c3a -- SSA (CIR\001) -> allocated IR (CIR\003).
 *
 * This is the first half of c3.  It parses each function, computes live
 * intervals and register homes, and serialises the decoded instructions
 * plus the allocation for the code generator (c3b).  Dumping the decoded
 * instruction array directly is what keeps c3b's parser tiny.
 */
#include <string.h>
#include <unistd.h>
#include <err.h>
#include "c3.h"

static void
skip_bytes (n)
int n;
{
	while (n-- > 0)
		(void) getb ();
}

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
			errx (1, "c3a: bad init chunk %#x", cnk);
		}
	}
}

static void
parse ()
{
	byte type, flags;
	word sym;

	if (lseek (0, 0L, SEEK_SET) != 0)
		err (1, "lseek");
	{
		char buf[4];
		get (buf, 4);
		if (memcmp (buf, "CIR\001", 4) != 0)
			errx (1, "c3a: invalid magic");
	}
	while (read (0, &type, 1) == 1) {
		switch (type) {
		case 'T':
			skip_bytes (getw ());
			break;
		case 'D':
			copy_data ();
			break;
		case 'F':
			flags = getb ();
			sym = getw ();
			if ((flags & 4) == 0) {
				if (getb () != 0xff)
					errx (1, "c3a: malformed F");
				md_b ('F');
				md_b (flags);
				md_w (sym);
				md_b (0xff);
			} else {
				read_body ();
				alloc_func (flags, sym);
			}
			break;
		default:
			errx (1, "c3a: invalid global %c", type);
		}
	}
}

int
main (argc, argv)
int	 argc;
char	**argv;
{
	(void) argc;
	(void) argv;

	md_b ('C'); md_b ('I'); md_b ('R'); md_b (3);
	parse ();
	md_flush ();
	return 0;
}
