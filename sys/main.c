#include <stdint.h>

extern void setldt ();
extern void putldt ();
extern void puttss ();
extern void putidt ();

extern char __user_begin[], __user_end[];

struct proc {
	struct proc	*p_next;
	int		 p_pid;
	char		*p_ldt;
};

struct user {
	struct proc	*u_proc;
	char		 u_kstack[128 - 4];
	int		 u_cs;
	int		 u_ss;
	int		 u_sp;
};

struct test {
	struct proc p;
	struct user u;
	char ldt[8 * 5];
	char data[512];
};

static struct test tests[2];

static void
init_test (t, pid)
struct test	*t;
int		 pid;
{
	t->p.p_pid = pid;
	t->p.p_ldt = t->ldt;
	t->u.u_proc = &t->p;
	t->u.u_cs = 0x08 | 7;
	t->u.u_ss = 0x10 | 7;
	t->u.u_sp = sizeof (t->u.u_kstack);

	putldt (t->ldt, 0x00, (long)&t->u, sizeof (struct user), 0x92);
	putldt (t->ldt, 0x08, (long)__user_begin, __user_end - __user_begin, 0xFA);
	putldt (t->ldt, 0x10, (long)t->data, sizeof (t->data), 0xF2);
	putldt (t->ldt, 0x18, 0xb8000L, 80 * 25 * 2, 0xF2);
}

static void
schedule ()
{
	struct user __far *u = (struct user __far *)0x00010000;
	struct proc *p, *n;

	p = u->u_proc;
	n = p->p_next;
}

int
syscall (code, a1, a2, a3)
int code, a1, a2, a3;
{
	switch (code) {
	case 0:
		*(volatile uint16_t __far *)0x00180000 = a1;
		return 0;
	case 1:
		schedule ();
		return 0;
	default:
		return -1;
	}
}

void
main ()
{
	extern void i_sys ();
	struct user *u;

	putidt (0x80 * 8, 0x08, i_sys, 0xE6);

	init_test (&tests[0], 0);
	init_test (&tests[1], 1);

	tests[0].p.p_next = &tests[1].p;
	tests[1].p.p_next = &tests[0].p;

	u = &tests[0].u;
	setldt (u->u_proc->p_ldt);
	puttss (u->u_kstack + sizeof (u->u_kstack));
	enter (u->u_cs, 0, u->u_ss, u->u_sp);
}
