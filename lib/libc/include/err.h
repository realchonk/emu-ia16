#ifndef FILE_ERR_H
#define FILE_ERR_H
#include <cdefs.h>

__dead void	err ();
__dead void	errx ();
__dead void	verr ();
__dead void	verrx ();
void		warn ();
void		warnx ();
void		vwarn ();
void		vwarnx ();

#endif /* FILE_ERR_H */
