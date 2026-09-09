#include <string.h>
#include "c0.h"

/*
 * stmt.c -- function definitions and statement lowering for c0.  The
 * IR is streamed: every statement writes its bytes as it is parsed,
 * and control flow (if, while, do, for, switch) is lowered into
 * labels, jumps and branches referencing symbols, which are
 * position-independent.  A switch dispatches through a compare chain
 * placed after its body and jumped over on entry.  The F record is
 * finished in fdefend, with the parameter and local sizes trailing
 * the code (see ir-fmt).
 */


struct symb	*curfunc;

static char	*labels[NLAB];		/* raw names, for the checks */
static int	 nlabels;
static char	*golist[NLAB];
static int	 ngolist;
static int	 loopdepth, swdepth;

static struct swcase	 cases[NCASE];
static int		 ncases;
static char		 labarena[NLABCH];
static int		 nlabaren;
static int		 brklab[NNEST], contlab[NNEST];
static int		 nbrk, ncont;
static struct {
	int		 lab1, lab2;	/* do: body label; for: cond label */
	struct node	*step;		/* for: the step expression */
} loopst[NNEST];
static int		 nloopst;
static struct {
	struct node	*t;		/* the switch temporary */
	struct swcase	*chain;
	struct swcase	**tail;
	int		 dflt;
	int		 endlab;
	int		 chainlab;
} swst[NSW];
static int		 nsw;
static int		 iflab[NNEST][2];	/* [0]: else/end, [1]: end */
static int		 nif;
static int		 labgen;
static int		 swtemp;

static char *
labstore (s)
char *s;
{
	char *p = &labarena[nlabaren];
	int len = strlen (s) + 1;

	if (nlabaren + len > NLABCH)
		error ("function too large");
	memcpy (p, s, len);
	nlabaren += len;
	return p;
}

/* generated label number, unique in the whole file */
static int
newlab ()
{
	return ++labgen;
}

/* a user label, mangled with the function name to stay unique */
static char *
userlab (name)
char *name;
{
	char buf[80], *p = buf;
	char *fn = curfunc->s_name;

	while (*fn != '\0')
		*p++ = *fn++;
	*p++ = '.';
	while (*name != '\0')
		*p++ = *name++;
	*p = '\0';
	return labstore (buf);
}

/* ---------------- function definitions ---------------- */

void
fdef_dcl (d)
struct dcl *d;
{
	struct type *tp = dcltype (curbase (), d);
	struct symb *sp, *p;
	struct symb *plist[NLOC];
	char *name = dclname (d);
	int sc = curd_sc ();
	int np = 0;

	if (name == NULL) {
		typerr ("function name omitted");
		name = "";
	}
	if (tp->t_op != T_FUNC) {
		/* a non-function declarator given a body; treat as
		   returning the declared type */
		tp = mktype (T_FUNC, tp, 0, NULL, NULL);
	}
	dcl_reset ();

	sp = lookup (name);
	if (sp != NULL) {
		if (sp->s_tp->t_op != T_FUNC)
			typerr ("redeclaration of '%s'", name);
		else if (!compat (sp->s_tp->t_tp, tp->t_tp))
			typerr ("conflicting return type for '%s'", name);
		sp->s_tp = tp;
	} else {
		sp = install (name, sc == 0 ? SC_EXTERN : sc);
		sp->s_tp = tp;
	}
	emit_fhead (name, sc);

	curfunc = sp;
	nlabels = ngolist = 0;
	nlocidx = 0;
	nlabaren = 0;
	ncases = 0;
	blkpush ();
	/* the parameter list is stored back to front; install the
	   parameters in declaration order so they take the first
	   local slots */
	for (p = tp->t_memb; p != NULL && np < NLOC; p = p->s_next)
		plist[np++] = p;
	while (--np >= 0)
		if (insparam (plist[np]->s_name) == NULL)
			typerr ("duplicate parameter '%s'",
				plist[np]->s_name);
	nargsloc = nlocidx;
}

void
fdefend ()
{
	int i, j;

	for (i = 0; i < ngolist; ++i) {
		for (j = 0; j < nlabels; ++j)
			if (strcmp (golist[i], labels[j]) == 0)
				break;
		if (j == nlabels)
			typerr ("undefined label '%s'", golist[i]);
	}
	emit_ftail (curfunc->s_tp);
	blkpop ();
	for (i = 0; i < nlocidx; ++i)
		freesymb (ltab[i]);
	nlocidx = 0;
	nbrk = ncont = 0;
	nloopst = 0;
	nsw = 0;
	nif = 0;
	curfunc = NULL;
	dcl_reset ();
	expr_reset ();
	held_reset ();
}

void
loopbegin ()
{
	++loopdepth;
}

void
loopend ()
{
	--loopdepth;
}

void
swbegin ()
{
	++swdepth;
}

void
swend ()
{
	--swdepth;
}

/* ---------------- statements ---------------- */

void
stmtx (e)
struct node *e;
{
	emexpr (exproper (e));
	expr_reset ();
}

void
stmtcond (e)
struct node *e;
{
	exproper (e);
	if (e != NULL && !isscalar (decay (e->n_tp)))
		typerr ("controlling expression must be scalar");
}

void
stmtfor (e1, e2, e3)
struct node *e1, *e2, *e3;
{
	exproper (e1);
	exproper (e2);
	exproper (e3);
	if (e2 != NULL && !isscalar (decay (e2->n_tp)))
		typerr ("controlling expression must be scalar");
}

void
stmtswitch (e)
struct node *e;
{
	exproper (e);
	if (e != NULL && !isarith (decay (e->n_tp)))
		typerr ("switch expression must be arithmetic");
}

/*
 * if (c) A else B lowers to
 *	B .Lt .Lc c; .Lt: A; J .Lend; .Lc: B; .Lend:
 * (the else label doubles as the end label without an else).
 */
void
sif (cond)
struct node *cond;
{
	int lt, lf;

	lt = newlab ();
	lf = newlab ();
	iflab[nif][0] = lf;
	iflab[nif][1] = newlab ();		/* .Lend */
	++nif;

	embr (lt, lf, cond);
	expr_reset ();
	emlab (lt);
}

void
sifend ()
{
	--nif;
	emlab (iflab[nif][0]);
}

void
selse ()
{
	emjump (iflab[nif - 1][1]);
	emlab (iflab[nif - 1][0]);
}

void
sifendelse ()
{
	--nif;
	emlab (iflab[nif][1]);
}

/*
 * while (c) A lowers to
 *	.Lc: B .Lb .Le c; .Lb: A; J .Lc; .Le:
 */
void
swhile (cond)
struct node *cond;
{
	int lc, lb;

	lc = newlab ();
	lb = newlab ();
	brklab[nbrk++] = newlab ();		/* .Le */
	contlab[ncont++] = lc;

	emlab (lc);
	embr (lb, brklab[nbrk - 1], cond);
	expr_reset ();
	emlab (lb);
}

void
swhileend ()
{
	emjump (contlab[ncont - 1]);
	emlab (brklab[--nbrk]);
	--ncont;
}

/*
 * do A while (c) lowers to
 *	.Lb: A; .Lc: B .Lb .Le c; .Le:
 */
void
sdo ()
{
	int lb, lc;

	lb = newlab ();
	lc = newlab ();
	brklab[nbrk++] = newlab ();		/* .Le */
	contlab[ncont++] = lc;
	loopst[nloopst].lab1 = lb;
	++nloopst;

	emlab (lb);
}

void
sdoend (cond)
struct node *cond;
{
	--nloopst;
	emlab (contlab[ncont - 1]);
	embr (loopst[nloopst].lab1, brklab[nbrk - 1], cond);
	expr_reset ();
	emlab (brklab[--nbrk]);
	--ncont;
}

/*
 * for (a; c; s) A lowers to
 *	a; .Lc: B .Lb .Le c; .Lb: A; .Ls: s; J .Lc; .Le:
 */
void
sfor (e1, e2, e3)
struct node *e1, *e2, *e3;
{
	int lc, lb, ls;

	if (e1 != NULL) {
		emexpr (e1);
		expr_reset ();
	}
	lc = newlab ();
	lb = newlab ();
	ls = newlab ();
	brklab[nbrk++] = newlab ();		/* .Le */
	contlab[ncont++] = ls;
	loopst[nloopst].lab1 = ls;
	loopst[nloopst].lab2 = lc;
	loopst[nloopst].step = e3 == NULL ? NULL : holdcopy (e3);
	++nloopst;
	expr_reset ();

	emlab (lc);
	if (e2 != NULL)
		embr (lb, brklab[nbrk - 1], e2);
	else
		emjump (lb);
	emlab (lb);
}

void
sforend ()
{
	--nloopst;
	emlab (loopst[nloopst].lab1);		/* .Ls */
	if (loopst[nloopst].step != NULL)
		emexpr (loopst[nloopst].step);
	emjump (loopst[nloopst].lab2);		/* .Lc */
	emlab (brklab[--nbrk]);
	--ncont;
	expr_reset ();
}

/*
 * switch (c) stores c in a hidden local, jumps over the body to the
 * compare chain, which sswitchend writes after it:
 *	t = c; J .Lch; .Lk: ...; J .Lend; .Lch: (t==k?)...; .Lend:
 */
void
sswitch (cond)
struct node *cond;
{
	struct symb *sp;
	struct node *t;
	struct type *tp;
	char buf[16], *p = buf;

	tp = decay (cond->n_tp);
	if (tysize (tp) < 2)
		tp = btype (BT_INT);
	*p++ = '.';
	*p++ = 's';
	*p++ = 'w';
	p = numstr (p, ++swtemp);
	*p = '\0';
	sp = install (labstore (buf), SC_AUTO);
	sp->s_tp = tp;
	t = nlocal (sp);

	emexpr (nasgn ('=', t, cond));
	expr_reset ();
	if (nsw >= NSW)
		error ("switches nested too deeply");
	swst[nsw].t = holdcopy (t);
	swst[nsw].chain = NULL;
	swst[nsw].tail = &swst[nsw].chain;
	swst[nsw].dflt = 0;
	swst[nsw].endlab = newlab ();
	swst[nsw].chainlab = newlab ();
	emjump (swst[nsw].chainlab);
	++nsw;
	brklab[nbrk++] = swst[nsw - 1].endlab;
}

void
stmtcase (e)
struct node *e;
{
	struct swcase *c;
	long v;
	int ok;

	if (swdepth == 0) {
		typerr ("case outside of switch");
		return;
	}
	if (nsw == 0)
		error ("internal: switch stack underflow");
	if (e != NULL) {
		v = fold (exproper (e), &ok);
		if (!ok) {
			typerr ("case label is not constant");
			v = 0;
		}
	} else
		v = 0;
	if (ncases >= NCASE)
		error ("too many cases");
	c = &cases[ncases++];
	c->next = NULL;
	c->val = (int) v;
	c->lab = newlab ();
	*swst[nsw - 1].tail = c;
	swst[nsw - 1].tail = &c->next;

	emlab (c->lab);
}

void
stmtdflt ()
{
	if (swdepth == 0) {
		typerr ("default outside of switch");
		return;
	}
	if (nsw == 0)
		error ("internal: switch stack underflow");
	swst[nsw - 1].dflt = newlab ();
	emlab (swst[nsw - 1].dflt);
}

void
sswitchend ()
{
	--nsw;
	emjump (swst[nsw].endlab);
	emlab (swst[nsw].chainlab);
	emswch (swst[nsw].t, swst[nsw].chain, swst[nsw].dflt,
		swst[nsw].endlab);
	emlab (swst[nsw].endlab);
	--nbrk;
	expr_reset ();
}

void
stmtret (e)
struct node *e;
{
	struct type *rt;

	exproper (e);
	if (curfunc == NULL)
		return;
	rt = curfunc->s_tp->t_tp;
	if (e == NULL) {
		emret (NULL, NULL);
		if (isarith (rt) || isptr (rt)) {
			/* plain `return;' from a non-void function --
			   tolerated, as the C compiler did */
		}
		return;
	}
	if (!compat (rt, decay (e->n_tp)))
		typerr ("incompatible return value");
	emret (rt, e);
	expr_reset ();
}

void
stmtgoto (name)
char *name;
{
	if (ngolist < NLAB)
		golist[ngolist++] = name;
	emusym (userlab (name), 'J');
}

void
stmtlabel (name)
char *name;
{
	int i;

	for (i = 0; i < nlabels; ++i)
		if (strcmp (labels[i], name) == 0) {
			typerr ("duplicate label '%s'", name);
			return;
		}
	if (nlabels < NLAB)
		labels[nlabels++] = name;
	emusym (userlab (name), 'L');
}

void
stmtbrk ()
{
	if (loopdepth == 0 && swdepth == 0) {
		typerr ("break outside of loop or switch");
		return;
	}
	emjump (brklab[nbrk - 1]);
}

void
stmtcont ()
{
	if (loopdepth == 0) {
		typerr ("continue outside of loop");
		return;
	}
	emjump (contlab[ncont - 1]);
}
