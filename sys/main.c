#include <sys/queue.h>
#include <stddef.h>
#include <stdint.h>

extern void setldt ();
extern void putldt ();
extern void puttss ();
extern void putidt ();
extern int  save ();

extern char __user_begin[], __user_end[];

int
syscall (code, a1, a2, a3)
int code, a1, a2, a3;
{
	switch (code) {
	case 0:
		*(volatile uint16_t __far *)0x00180000 = a1;
		return 0;
	default:
		return -1;
	}
}

#define NPROC 64	/* maximum number of proc's */
#define NKSTACK	512	/* kernel stack size */
#define NUCORE	16	/* maximum number of user's in core */

/* p_stat */
#define SSLEEP	1
#define SWAIT	2
#define SRUN	3
#define SIDL	4
#define SZOMB	5
#define SSTOP	6

/* p_flag */
#define SLOAD	0x01
#define SSYS	0x02
#define SLOCK	0x04
#define SSWAP	0x08

struct proc {
	TAILQ_ENTRY (proc)	 p_queue;
	int			 p_stat;
	int			 p_flag;
	int			 p_pid;
	int			 p_ppid;
	struct user		*p_user;
} procs[NPROC];

struct user {
	struct proc		*u_proc;
	SLIST_ENTRY (user)	 u_free;	/* free list */
	void			*u_sp;
	int			 u_nldt;	/* size of u_ldt[] */
	char			 u_ldt[];
};

char stacks[NUCORE * NKSTACK];
static TAILQ_HEAD (, proc)	 runq, pfree;
static SLIST_HEAD (, user)	 ufree;
static struct proc		*current;

static void
init_proc ()
{
	struct proc	*p;
	struct user	*u;
	char		*s;
	int		 i;

	TAILQ_INIT (&runq);
	TAILQ_INIT (&pfree);
	SLIST_INIT (&ufree);

	for (i = 1, p = procs + 1; i < NPROC; ++i, ++p) {
		TAILQ_INSERT_TAIL (&pfree, p, p_queue);
	}

	for (i = 1, s = stacks + NKSTACK; i < NUCORE; ++i, s += NKSTACK) {
		u = (struct user *)s;
		SLIST_INSERT_HEAD (&ufree, u, u_free);
	}
}

char *
memcpy (dst, src, num)
void		*dst;
const void	*src;
size_t		 num;
{
	char		*d;
	const char	*s;

	for (d = dst, s = src; num != 0; ++d, ++s, --num)
		*d = *s;

	return dst;
}

static int
newproc ()
{
	struct proc 	*p;
	struct user	*u, *cu;
	char		*sp, *top, *csp, *d, *src;
	int		 pid;

	cu = current->p_user;

	if (TAILQ_EMPTY (&pfree) || SLIST_EMPTY (&ufree))
		return -1;

	p = TAILQ_FIRST (&pfree);
	u = SLIST_FIRST (&ufree);
	TAILQ_REMOVE (&pfree, p, p_queue);
	SLIST_REMOVE_HEAD (&ufree, u_free);

	/* copy the user area, including the LDT */
	memcpy (u, cu, sizeof (struct user) + cu->u_nldt);

	u->u_proc	= p;
	p->p_user	= u;
	p->p_pid	= p - procs;
	p->p_ppid	= current->p_pid;
	p->p_stat	= SRUN;
	p->p_flag	= current->p_flag;
	pid		= p->p_pid;

	/*
	 * Save our kernel context inside save().  The parent returns here
	 * with 0; the child will later be resumed by swtch() at exactly the
	 * same point, but with its saved ax set to 1, so it takes the
	 * "return 0" branch.
	 */
	if (save (&cu->u_sp))
		return 0;

	/*
	 * The child needs a copy of the active part of our kernel stack,
	 * otherwise it cannot return through the same call chain
	 * (i_sys -> syscall -> newproc).
	 *
	 * Do the copy with a plain loop, not memcpy(): memcpy() would use
	 * the very stack we are copying for its own call frame, overwriting
	 * the saved return address in the process.
	 */
	sp  = cu->u_sp;
	top = (char *)cu + NKSTACK;
	csp = (char *)u + (sp - (char *)cu);
	for (d = csp, src = sp; src != top; ++d, ++src)
		*d = *src;

	/*
	 * Pointers saved in the stack (at least the frame pointer) are
	 * still absolute addresses in the parent's stack.  Move the frame
	 * pointer to the corresponding place in the child's stack.
	 */
	*(int *)(csp + 6) += (char *)u - (char *)cu;

	/* make save() appear to have returned 1 in the child */
	*(int *)csp = 1;

	u->u_sp = csp;

	TAILQ_INSERT_HEAD (&runq, p, p_queue);

	return pid;
}

static void
schedule ()
{
	extern void swtch ();
	struct proc *p, *o;
	
	if (TAILQ_EMPTY (&runq))
		return;

	p = TAILQ_FIRST (&runq);
	o = current;

	if (p == o)
		return;

	/* move to end */
	TAILQ_REMOVE (&runq, p, p_queue);
	TAILQ_INSERT_TAIL (&runq, p, p_queue);

	current = p;
	swtch (&o->p_user->u_sp, p->p_user->u_sp);
}

void
main ()
{
	extern void i_sys ();
	struct proc	*p;
	struct user	*u;

	init_proc ();

	p = &procs[0];
	p->p_stat	= SRUN;
	p->p_flag	= SSYS | SLOAD;
	p->p_pid	= 0;
	p->p_ppid	= 0;
	u = p->p_user	= (struct user *)&stacks[0];
	u->u_proc	= p;
	u->u_nldt	= 0;
	current		= p;
	setldt (&u->u_ldt, u->u_nldt);

	if (newproc ()) {
		syscall (0, 0x0700 | 'Z');
		while (1)
			schedule ();
	} else {
		while (1)
			schedule ();
	}
}
