#include <unistd.h>
#include <string.h>
#include <stdlib.h>
#include <stdio.h>
#include <fcntl.h>
#include <err.h>

#define IS	(sizeof (struct inode))
#define BPS	(512 * 8)	/* bits per sector */
#define IPS	(512 / IS)	/* inodes per sector */
#define BPI	8		/* assume 8 data blocks per inode on average */

struct fs {
	char	fs_magic[4];	/* SUFS */
	int	fs_size;	/* total number of sectors */
	int	fs_isize;	/* number of inodes */
	int	fs_dsize;	/* number of data blocks */
	int	fs_iboff;	/* inode bitmap offset */
	int	fs_dboff;	/* data bitmap offset*/
	int	fs_ioff;	/* inodes offset */
	int	fs_doff;	/* data blocks offset */
	int	fs_ifree;	/* number of free inodes */
	int	fs_dfree;	/* number of free data blocks */
};

struct inode {
	int	i_mode;		/* type and permissions */
	char	i_nlink;	/* number of hard links */
	char	i_uid;		/* owner's user ID */
	char	i_gid;		/* owner's group ID */
	char	i_size0;	/* size[23:16] */
	int	i_size1;	/* size[15:0] */
	long	i_atime;	/* access time */
	long	i_mtime;	/* modification time */
	int	i_blocks[8];	/* filesystem blocks */
};

struct template {
	int	d_cino;
	char	d_cname[14];
	int	d_pino;
	char	d_pname[14];
};

static struct fs	fs;
static int		fd, vflag = 0;
static char		buf[512];

static int
usage ()
{
	fputs ("usage: newfs [-s size] device\n", stderr);
	return 1;
}

static void
swrite (off, buf)
int		 off;
const char	*buf;
{
	if (vflag > 1)
		warnx ("swrite(%d)", off);
	if (pwrite (fd, buf, 512, off * 512L) != 512)
		err (1, "swrite(%d)", off);
}

static void
sread (off, buf)
int	 off;
char	*buf;
{
	if (vflag > 1)
		warnx ("sread(%d)", off);
	if (pread (fd, buf, 512, off * 512L) != 512)
		err (1, "sread(%d)", off);
}

static void
sfill (off, val)
int	off, val;
{
	memset (buf, val, 512);
	swrite (off, buf);
}

static void
winode (ino, in)
int		 ino;
struct inode	*in;
{
	int	bs, bo, is, io;

	if (vflag > 1)
		warnx ("winode(%d)", ino);

	/* mark as used */
	bs = ino / BPS;
	bo = ino % BPS;
	sread (fs.fs_iboff + bs, buf);
	buf[bo / 8] &= ~(1 << (bo & 7));
	swrite (fs.fs_iboff + bs, buf);

	/* write inode to disk */
	is = ino / IPS;
	io = ino % IPS;
	sread (fs.fs_ioff + is, buf);
	memcpy (buf + io * IS, in, IS);
	swrite (fs.fs_ioff + is, buf);
}

static void
wdata (bno)
int		 bno;
{
	int bs, bo;

	if (bno < fs.fs_doff || bno >= fs.fs_size)
		errx (1, "wdata(%d)", bno);

	swrite (bno, buf);

	/* mark as used */
	bno -= fs.fs_doff;
	bs = bno / BPS;
	bo = bno % BPS;
	sread (fs.fs_dboff + bs, buf);
	buf[bo / 8] &= ~(1 << (bo & 7));
	swrite (fs.fs_dboff + bs, buf);
}

int
main (argc, argv)
int	  argc;
char	**argv;
{
	struct inode	 in;
	struct template	 tmp;
	char		*path;
	int		 i, o, option, size = 0;
	int		 nib, ndb, nis, bno;

	while ((option = getopt (argc, argv, "s:v")) != -1) {
		switch (option) {
		case 's':
			size = atoi (optarg);
			if (size == 0)
				errx (1, "invalid size: %s", optarg);
			break;
		case 'v':
			++vflag;
			break;
		default:
			return usage ();
		}
	}

	argv += optind;
	argc -= optind;

	if (argc != 1)
		return usage ();

	path	= argv[0];
	fd	= open (path, O_RDWR);
	if (fd < 0)
		err (1, "%s", path);

	if (size == 0) {
		/* TODO: stat(2) */
		errx (1, "automatic size detection not implemented");
	}

	/* zero out the first 64K */
	for (i = 0; i < 128; ++i)
		sfill (i, 0);

	/* write superblock */
	o = 128 + 1;	/* boot blocks + superblock */
	fs.fs_magic[0]	= 'S';
	fs.fs_magic[1]	= 'U';
	fs.fs_magic[2]	= 'F';
	fs.fs_magic[3]	= 'S';
	fs.fs_iboff	= 128 + 1;
	fs.fs_size	= size;
	size -= o;

	if (size < 64)
		errx (1, "fs too small");

	fs.fs_isize	= size / BPI;
	fs.fs_iboff	= o;
	nib		= (fs.fs_isize + BPS - 1) / BPS;
	nis		= fs.fs_isize * IS / 512;
	o		+= nib;
	size		-= nib + nis;
	fs.fs_dboff	= o;
	fs.fs_dsize	= fs.fs_size - o;
	ndb		= (fs.fs_dsize + BPS - 1) / BPS;
	fs.fs_dsize	-= ndb;
	o		+= ndb;
	size		-= ndb;
	fs.fs_ioff	= o;
	o		+= nis;
	fs.fs_doff	= o;
	fs.fs_dsize	= size;
	memset (buf, 0, 512);
	memcpy (buf, &fs, sizeof (fs));
	swrite (128, buf);

	/* clear out inode bitmaps */
	for (i = 0; i < nib; ++i)
		sfill (i + fs.fs_iboff, 0xff);

	/* clear out data block bitmaps */
	for (i = 0; i < ndb; ++i)
		sfill (i + fs.fs_dboff, 0xff);

	fs.fs_ifree	= fs.fs_isize;
	fs.fs_dfree	= fs.fs_dsize;

	/* set up invalid inode */
	memset (&in, 0, IS);
	winode (0, &in);
	--fs.fs_ifree;

	/* set up root directory data */
	bno		= fs.fs_doff;
	memset (buf, 0, 512);
	tmp.d_cino	= 1;
	strncpy (tmp.d_cname, ".", 14);
	tmp.d_pino	= 1;
	strncpy (tmp.d_cname, "..", 14);
	memcpy (buf, &tmp, sizeof (tmp));
	wdata (bno);
	--fs.fs_dfree;

	/* set up root inode */
	memset (&in, 0, IS);
	in.i_mode	= 040755;
	in.i_nlink	= 2;
	in.i_size1	= 512;
	in.i_blocks[0]	= bno;
	winode (1, &in);
	--fs.fs_ifree;

	if (vflag) {
		warnx ("fs_size  = %d", fs.fs_size);
		warnx ("fs_isize = %d", fs.fs_isize);
		warnx ("fs_dsize = %d", fs.fs_dsize);
		warnx ("fs_iboff = %d", fs.fs_iboff);
		warnx ("fs_dboff = %d", fs.fs_dboff);
		warnx ("fs_ioff  = %d", fs.fs_ioff);
		warnx ("fs_doff  = %d", fs.fs_doff);
		warnx ("fs_ifree = %d", fs.fs_ifree);
		warnx ("fs_dfree = %d", fs.fs_dfree);
	}

	return 0;
}
