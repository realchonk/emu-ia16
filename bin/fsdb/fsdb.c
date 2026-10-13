#include <assert.h>
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

struct dirent {
	int	d_ino;
	char	d_name[14];
};

static struct fs	fs;
static int		fd, ino = 1, lino = 1, vflag = 0;
static char		buf[512];
static int		dbuf[256];

/* FS UTILS */

static void
swrite (off, buf)
int	 off;
void	*buf;
{
	if (vflag > 1)
		warnx ("swrite(%d)", off);
	if (pwrite (fd, buf, 512, off * 512L) != 512)
		err (1, "swrite(%d)", off);
}

static void
sbwrite ()
{
	memcpy (buf, &fs, sizeof (fs));
	swrite (128, buf);
}

static void
sread (off, buf)
int	 off;
void	*buf;
{
	if (vflag > 1)
		warnx ("sread(%d)", off);
	if (pread (fd, buf, 512, off * 512L) != 512)
		err (1, "sread(%d)", off);
}

/* check if an inode is free */
static int
ifree (ino)
int	ino;
{
	int	bs, bo;
	char	m;

	bs = ino / BPS;
	bo = ino % BPS;
	sread (fs.fs_iboff + bs, buf);
	m = 1 << (bo & 7);
	return (buf[bo / 8] & m) == m;
}

/* mark an inode as free/used */
static void
minode (ino, free)
int	ino, free;
{
	int	bs, bo;
	char	m;

	if (vflag > 1)
		warnx ("minode(%d, %d)", ino, free);

	/* mark as used */
	bs = ino / BPS;
	bo = ino % BPS;
	sread (fs.fs_iboff + bs, buf);
	m = 1 << (bo & 7);
	if (free) {
		buf[bo / 8] |= m;
	} else {
		buf[bo / 8] &= ~m;
	}
	swrite (fs.fs_iboff + bs, buf);

}

static void
rwinode (ino, in, rw)
int		 ino, rw;
struct inode	*in;
{
	int	is, io;

	if (vflag > 1)
		warnx ("winode(%d)", ino);

	/* write inode to disk */
	is = ino / IPS;
	io = ino % IPS;
	sread (fs.fs_ioff + is, buf);
	if (rw) {
		memcpy (buf + io * IS, in, IS);
		swrite (fs.fs_ioff + is, buf);
	} else {
		memcpy (in, buf + io * IS, IS);
	}
}

static void
rinode (ino, in)
int		 ino;
struct inode	*in;
{
	rwinode (ino, in, 0);
}

static void
winode (ino, in)
int		 ino;
struct inode	*in;
{
	rwinode (ino, in, 1);
}

static long
isize (in)
const struct inode	*in;
{
	return ((long)in->i_size0 << 16) | in->i_size1;
}

static void
ichlen (in, sz)
struct inode	*in;
long		 sz;
{
	in->i_size0 = (sz >> 16) & 0xff;
	in->i_size1 = sz & 0xffff;
}

static int
balloc ()
{
	int	i, j, k, m, ndb, bno;

	ndb = (fs.fs_dsize + BPS - 1) / BPS;

	for (i = 0; i < ndb; ++i) {
		sread (i + fs.fs_dboff, buf);
		for (j = 0; j < 512; ++j) {
			if (buf[j] == 0x00)
				continue;

			for (k = 0, m = 1; k < 8; ++k, m <<= 1) {
				if ((buf[j] & m) != m)
					continue;

				buf[j] &= ~m;
				swrite (i + fs.fs_dboff, buf);

				bno = fs.fs_doff + i * BPS + j * 8 + k;

				memset (buf, 0, 512);
				swrite (bno, buf);

				--fs.fs_dfree;
				sbwrite ();

				return bno;
			}
		}
	}

	errx (1, "balloc(): no space left on device");
}

static int
iblock (ino, in, bno, alloc)
struct inode	*in;
int		 ino, bno, alloc;
{
	int b, o;

	if (bno < 6) {
		b = in->i_blocks[bno];
		if (b != 0 || !alloc)
			return b;
		b = in->i_blocks[bno] = balloc ();
		winode (ino, in);
		return b;
	} else if (bno < (6 + 256)) {
		if (in->i_blocks[6] == 0) {
			if (!alloc)
				return 0;
			in->i_blocks[6] = balloc ();
			winode (ino, in);
		}

		sread (in->i_blocks[6], dbuf);
		o = bno - 6;
		if (dbuf[o] != 0 || !alloc)
			return dbuf[o];

		b = dbuf[o] = balloc ();
		swrite (in->i_blocks[6], dbuf);
		return b;
	} else {
		if (in->i_blocks[7] == 0) {
			if (!alloc)
				return 0;
			in->i_blocks[7] = balloc ();
			winode (ino, in);
		}

		sread (in->i_blocks[7], dbuf);
		o = (bno - 256 - 6) / 256;
		if (dbuf[o] == 0) {
			if (!alloc)
				return 0;
			dbuf[o] = balloc ();
			swrite (in->i_blocks[7], dbuf);
		}
		b = dbuf[o];
		sread (b, dbuf);
		o = (bno - 256 - 6) % 256;
		if (dbuf[o] != 0 || !alloc)
			return dbuf[o];

		dbuf[o] = balloc ();
		swrite (b, dbuf);
		return dbuf[o];
	}
}

static int
ilookup (in, name, off)
struct inode	*in;
const char	*name;
long		*off;
{
	struct dirent	*ent;
	int		 b, bno;
	long		 o, i, sz, ff = 0;

	sz = isize (in);
	
	for (i = 0, bno = 0; i < sz; i += 512, ++bno) {
		b = iblock (ino, in, bno, 0);
		if (b == 0) {
			warnx ("directory has hole in block %d", bno);
			continue;
		}

		sread (b, buf);

		for (o = 0; o < 512; o += 16) {
			ent = (struct dirent *)&buf[o];

			if (ent->d_ino == 0 && ff == 0) {
				ff = i + o;
				continue;
			}

			if (ent->d_ino == 0 || strncmp (name, ent->d_name, 14) != 0)
				continue;

			if (off != NULL)
				*off = i + o;

			return ent->d_ino;
		}
	}

	*off = ff;

	return 0;
}

/* COMMANDS */

#define DELIM " \t"

static int
cmd_exit (cmd)
char	**cmd;
{
	(void)cmd;
	exit (0);
}

static int
cmd_inode (cmd)
char	**cmd;
{
	char	*inostr, prompt[32];

	inostr = strsep (cmd, DELIM);
	if (inostr == NULL) 
		return -1;

	lino = ino;
	ino = atoi (inostr);

	if (!ifree (ino))
		return 0;

	fprintf (stderr, "mark inode %d as used [Yn]? ", ino);
	if (fgets (prompt, sizeof (prompt), stdin) == NULL)
		return -1;

	switch (prompt[0]) {
	case 'y':
	case 'Y':
	case '\0':
	case '\n':
		minode (ino, 0);
		--fs.fs_ifree;
		sbwrite ();
		break;
	default:
		break;
	}

	return 0;
}

static int
cmd_back (cmd)
char	**cmd;
{
	int t;
	(void)cmd;

	t	= ino;
	ino	= lino;
	lino	= t;

	return 0;
}

static int
cmd_print (cmd)
char	**cmd;
{
	struct inode	in;
	int		i;

	(void)cmd;

	rinode (ino, &in);

	printf ("i_mode      = %o\n", in.i_mode);
	printf ("i_nlink     = %d\n", in.i_nlink);
	printf ("i_uid       = %d\n", in.i_uid);
	printf ("i_gid       = %d\n", in.i_gid);
	printf ("i_size      = %ld\n", isize (&in));
	printf ("i_atime     = %ld\n", in.i_atime);
	printf ("i_mtime     = %ld\n", in.i_mtime);
	for (i = 0; i < 8; ++i)
		printf ("i_blocks[%d] = 0x%x\n", i, in.i_blocks[i]);

	return 0;
}

static int
cmd_chmod (cmd)
char	**cmd;
{
	struct inode	 in;
	char		*modestr, *endp;
	int		 mode;

	modestr = strsep (cmd, DELIM);
	if (modestr == NULL || *modestr == '\0')
		return -1;

	mode = strtoi (modestr, &endp, 8);
	if (*endp != '\0') {
		warnx ("invalid mode: %s", modestr);
		return -1;
	}

	rinode (ino, &in);
	in.i_mode = (in.i_mode & ~07777) | (mode & 07777);
	winode (ino, &in);

	return 0;
}

static int
cmd_chtype (cmd)
char	**cmd;
{
	struct inode	 in;
	char		*typestr;
	int		 type;

	typestr = strsep (cmd, DELIM);
	if (typestr == NULL || *typestr == '\0')
		return -1;

	if (strcmp (typestr, "dir") == 0) {
		type = 040000;
	} else if (strcmp (typestr, "file") == 0) {
		type = 0100000;
	} else {
		warnx ("invalid type: %s", typestr);
		return -1;
	}

	rinode (ino, &in);
	in.i_mode = (in.i_mode & 07777) | type;
	winode (ino, &in);

	return 0;
}

static int
cmd_chown (cmd)
char	**cmd;
{
	struct inode	 in;
	char		*uidstr;
	int		 uid;

	uidstr = strsep (cmd, DELIM);
	if (uidstr == NULL || *uidstr == '\0')
		return -1;

	uid = atoi (uidstr);

	rinode (ino, &in);
	in.i_uid = uid;
	winode (ino, &in);

	return 0;
}

static int
cmd_chgrp (cmd)
char	**cmd;
{
	struct inode	 in;
	char		*gidstr;
	int		 gid;

	gidstr = strsep (cmd, DELIM);
	if (gidstr == NULL || *gidstr == '\0')
		return -1;

	gid = atoi (gidstr);

	rinode (ino, &in);
	in.i_gid = gid;
	winode (ino, &in);

	return 0;
}

static int
cmd_uplink (cmd)
char	**cmd;
{
	struct inode	in;

	(void)cmd;

	rinode (ino, &in);
	++in.i_nlink;
	winode (ino, &in);

	return 0;
}

static int
cmd_downlink (cmd)
char	**cmd;
{
	struct inode	in;

	(void)cmd;

	rinode (ino, &in);
	--in.i_nlink;
	winode (ino, &in);

	return 0;
}

static int
cmd_ls (cmd)
char	**cmd;
{
	struct inode	 in;
	struct dirent	*ent;
	long		 i, o, sz;
	int		 b, bno;

	(void)cmd;

	rinode (ino, &in);

	if ((in.i_mode & 0170000) != 040000) {
		warnx ("not a directory");
		return 0;
	}

	sz = isize (&in);

	for (i = 0, bno = 0; i < sz; i += 512, ++bno) {
		b = iblock (ino, &in, bno, 0);
		if (b == 0) {
			warnx ("directory with holes");
			continue;
		}

		sread (b, buf);

		for (o = 0; o < 512; o += 16) {
			ent = (struct dirent *)&buf[o];
			if (ent->d_ino == 0)
				continue;

			printf ("%-5d %.14s\n", ent->d_ino, ent->d_name);
		}
	}

	return 0;
}

static int
cmd_ln (cmd)
char	**cmd;
{
	struct inode	 in;
	struct dirent	*ent;
	long		 o, sz;
	int		 b, nino;
	char		*name, *inostr;

	inostr = strsep (cmd, DELIM);
	if (inostr == NULL || *inostr == '\0')
		return -1;

	name = strsep (cmd, DELIM);
	if (name == NULL || *name == '\0')
		return -1;

	nino = atoi (inostr);
	rinode (ino, &in);

	if ((in.i_mode & 0170000) != 040000) {
		warnx ("not a directory");
		return 0;
	}

	if (ilookup (&in, name, &o) != 0) {
		warnx ("already exists");
		return 0;
	}

	b = iblock (ino, &in, (int)(o / 512), 1);
	assert (b != 0);

	sz = isize (&in);
	if (o >= sz) {
		ichlen (&in, (o + 512) & ~511);
		winode (ino, &in);
	}

	sread (b, buf);
	ent = (struct dirent *)&buf[o % 512];
	assert (ent->d_ino == 0);
	ent->d_ino = nino;
	strncpy (ent->d_name, name, 14);
	swrite (b, buf);

	return 0;
}

static int
cmd_rm (cmd)
char	**cmd;
{
	struct inode	 in;
	struct dirent	*ent;
	long		 o;
	int		 b;
	char		*name;

	name = strsep (cmd, DELIM);
	if (name == NULL || *name == '\0')
		return -1;

	rinode (ino, &in);

	if ((in.i_mode & 0170000) != 040000) {
		warnx ("not a directory");
		return 0;
	}

	if (ilookup (&in, name, &o) == 0) {
		warnx ("no such file");
		return 0;
	}

	b = iblock (ino, &in, (int)(o / 512), 0);
	assert (b != 0);

	sread (b, buf);
	ent = (struct dirent *)&buf[o % 512];
	memset (ent, 0, 16);
	swrite (b, buf);

	return 0;
}

static int
cmd_clri (cmd)
char	**cmd;
{
	struct inode	 in;
	char		*inostr;
	int		 i;

	inostr = strsep (cmd, DELIM);
	if (inostr == NULL) 
		return -1;

	i = atoi (inostr);
	if (ifree (i)) {
		warnx ("inode %d already free", i);
		return 0;
	}
	memset (&in, 0, sizeof (in));
	winode (i, &in);
	minode (i, 1);
	++fs.fs_ifree;
	sbwrite ();
	return 0;
}

static int
cmd_df (cmd)
char	**cmd;
{
	(void)cmd;

	printf ("inodes:      %d/%d free\n", fs.fs_ifree, fs.fs_isize);
	printf ("data blocks: %d/%d free\n", fs.fs_dfree, fs.fs_dsize);

	return 0;
}

static struct cmd {
	const char	*name;
	int		(*f)();
	const char	*usage;
} cmds[] = {
	{ "q",		cmd_exit,	"" },
	{ "quit",	cmd_exit,	"" },
	{ "exit",	cmd_exit,	"" },
	{ "i",		cmd_inode,	" i-number" },
	{ "ino",	cmd_inode,	" i-number" },
	{ "inode",	cmd_inode,	" i-number" },
	{ "b",		cmd_back,	"" },
	{ "back",	cmd_back,	"" },
	{ "p",		cmd_print,	"" },
	{ "print",	cmd_print,	"" },
	{ "active",	cmd_print,	"" },
	{ "chmod",	cmd_chmod,	" mode" },
	{ "chtype",	cmd_chtype,	" file|dir" },
	{ "chown",	cmd_chown,	" uid" },
	{ "chgrp",	cmd_chgrp,	" gid" },
	{ "uplink",	cmd_uplink,	"" },
	{ "downlink",	cmd_downlink,	"" },
	{ "ls",		cmd_ls,		"" },
	{ "ln",		cmd_ln,		" ino name" },
	{ "rm",		cmd_rm,		" name" },
	{ "clri",	cmd_clri,	" ino" },
	{ "df",		cmd_df,		"" },
	{ NULL,		NULL,		NULL },
};

static void
run (cmd)
char	*cmd;
{
	struct cmd	*c;
	char		*name;

	name = strsep (&cmd, DELIM);

	if (*name == '\0')
		return;

	for (c = cmds; c->name != NULL; ++c) {
		if (strcmp (name, c->name) == 0) {
			if (c->f (&cmd) != 0) {
				fprintf (stderr, "usage: %s%s\n", c->name, c->usage);
			}
			return;
		}
	}

	fprintf (stderr, "invalid command: %s\n", name);
}

/* MAIN */

static int
usage ()
{
	fputs ("usage: fsdb [-rv] -f fsname\n", stderr);
	return 1;
}

int
main (argc, argv)
int	  argc;
char	**argv;
{
	char	*path = NULL, cmd[80];
	int	 option, rflag = 0;

	while ((option = getopt (argc, argv, "f:rv")) != -1) {
		switch (option) {
		case 'f':
			path = optarg;
			break;
		case 'r':
			rflag = 1;
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

	if (argc != 0)
		return usage ();

	fd = open (path, rflag ? O_RDONLY : O_RDWR);
	if (fd < 0)
		err (1, "%s", path);

	/* read superblock */
	sread (128, buf);
	memcpy (&fs, buf, sizeof (fs));

	while (1) {
		fprintf (stderr, "fsdb (ino %d)> ", ino);
		if (fgets (cmd, sizeof (cmd), stdin) == NULL) {
			fputc ('\n', stderr);
			break;
		}

		cmd[strcspn (cmd, "\n")] = '\0';

		run (cmd);
	}


	close (fd);
	return 0;
}
