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

static struct fs	fs;
static int		fd, ino = 2;

static int
usage ()
{
	fputs ("usage: fsdb [-r] -f fsname\n", stderr);
	return 1;
}

#define DELIM " \t"

static int
cmd_exit (cmd)
char **cmd;
{
	exit (0);
}

static int
cmd_inode (cmd)
char **cmd;
{
	char	*inostr;

	inostr = strsep (cmd, DELIM);
	if (inostr == NULL) 
		return -1;

	ino = atoi (inostr);

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
	{ "ino",	cmd_inode,	" i-number" },
	{ "inode",	cmd_inode,	" i-number" },
	{ NULL,		NULL,		NULL },
};

static void
run (cmd)
char	*cmd;
{
	struct cmd	*c;
	char		*name;

	name = strsep (&cmd, DELIM);

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

int
main (argc, argv)
int	  argc;
char	**argv;
{
	char	*path = NULL, cmd[80];
	int	 option, rflag = 0;

	while ((option = getopt (argc, argv, "rf:")) != -1) {
		switch (option) {
		case 'f':
			path = optarg;
			break;
		case 'r':
			rflag = 1;
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
