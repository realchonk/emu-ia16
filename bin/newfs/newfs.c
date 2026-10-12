#include <unistd.h>
#include <string.h>
#include <stdlib.h>
#include <stdio.h>
#include <fcntl.h>
#include <err.h>

struct fs {
	char	fs_magic[4];	/* SUFS */
	int	fs_size;	/* total number of sectors */
	int	fs_isize;	/* number of inodes */
	int	fs_dsize;	/* number of data blocks */
	int	fs_iboff;	/* */
	int	fs_dboff;	/* */
	int	fs_ioff;	/* */
	int	fs_doff;	/* */
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

static struct fs	fs;
static int		fd;
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
	if (pwrite (fd, buf, 512, off * 512L) != 512)
		err (1, "swrite(%d)", off);
}

static void
sfill (off, val)
int	off, val;
{
	memset (buf, val, 512);
	swrite (off, buf);
}

int
main (argc, argv)
int	  argc;
char	**argv;
{
	char	*path;
	int	 i, o, option, size = 0;

	while ((option = getopt (argc, argv, "s:")) != -1) {
		switch (option) {
		case 's':
			size = atoi (optarg);
			if (size == 0)
				errx (1, "invalid size: %s", optarg);
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
	fd	= open (path, O_WRONLY);
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

	fs.fs_isize	= size / 16; /* assume 16 data blocks per inode on avg */
	warnx ("fs_isize = %d", fs.fs_isize);


	/*
	fs.fs_isize	= ;
	fs.fs_dsize	= ;
	fs.fs_iboff	= ;
	fs.fs_dboff	= ;
	fs.fs_ioff	= ;
	fs.fs_doff	= ;
	*/

	return 0;
}
