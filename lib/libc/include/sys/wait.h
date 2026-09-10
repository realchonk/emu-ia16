#ifndef FILE_SYS_WAIT_H
#define FILE_SYS_WAIT_H

#define WIFEXITED(ws) (((ws) & 0x7f) == 0)
#define WEXITSTATUS(ws) (((ws) >> 8) & 0xff)

int	wait ();

#endif /* FILE_SYS_WAIT_H */
