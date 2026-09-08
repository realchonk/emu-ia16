#include <string.h>
#include "c0.h"

/*
 * stmt.c -- function definitions and statement lowering for c0.  As
 * the body is parsed, statements are recorded in a flat list of
 * records: control flow (if, while, do, for, switch) is lowered into
 * labels, jumps and branches, with break and continue resolving to the
 * innermost loop or switch.  fdefend hands the record list, the
 * parameters and the locals to emit.c, which serializes the function.
 */


struct symb	*curfunc;

static char	*labels[NLAB];		/* raw names, for the checks */
static int	 nlabels;
static char	*golist[NLAB];
static int	 ngolist;
static int	 loopdepth, swdepth;

/* lowered statement records */
static struct srec	 srecs[NSREC];
static int		 nsrecs;
static struct swcase	 cases[NCASE];
static int		 ncases;
static char		 labarena[NLABCH];
static int		 nlabaren;
static char		*brklab[NNEST], *contlab[NNEST];
static int		 nbrk, ncont;
static struct {
	char		*lab1, *lab2;	/* do: body label; for: cond label */
	struct node	*step;		/* for: the step expression */
} loopst[NNEST];
static int		 nloopst;
static struct srec	*swst[NSW];
static int		 nsw;
static struct swcase	**swtail[NSW];
static char		*swdflt[NSW];
static char		*swendlab[NSW];
static char		*iflab[NNEST][2];	/* [0]: else/end, [1]: end */
static int		 nif;
static int		 labgen;
static int		 swtemp;

static struct srec *
addrec (kind)
int kind;
{
	struct srec *r;

	if (nsrecs >= NSREC)
		error ("function too large");
	r = &srecs[nsrecs++];
	r->kind = kind;
	r->r_e = NULL;
	r->r_lab = r->r_lf = r->r_dflt = NULL;
	r->r_t = NULL;
	r->r_cases = NULL;
	return r;
}

static void
addlab (lab)
char *lab;
{
	addrec (S_LABEL)->r_lab = lab;
}

static void
addjump (lab)
char *lab;
{
	addrec (S_JUMP)->r_lab = lab;
}

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

/* generated label, unique in the whole file */
static char *
newlab ()
{
	char buf[16], *p = buf;

	*p++ = '.';
	*p++ = 'L';
	p = numstr (p, ++labgen);
	*p = '\0';
	return labstore (buf);
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
	char *name = dclname (d);
	int sc = curd_sc ();

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

	curfunc = sp;
	nlabels = ngolist = 0;
	blkpush ();
	for (p = tp->t_memb; p != NULL; p = p->s_next) {
		if (insparam (p->s_name) == NULL)
			typerr ("duplicate parameter '%s'", p->s_name);
	}
}

void
fdefend ()
{
	static struct symb *ltab[NLOC];
	int nltab;
	int i, j;

	for (i = 0; i < ngolist; ++i) {
		for (j = 0; j < nlabels; ++j)
			if (strcmp (golist[i], labels[j]) == 0)
				break;
		if (j == nlabels)
			typerr ("undefined label '%s'", golist[i]);
	}
	nltab = getlocals (ltab);
	emit_func (curfunc->s_name, curfunc->s_sc, curfunc->s_tp, srecs,
		   nsrecs, ltab, nltab);
	blkpop ();
	curfunc = NULL;
	dcl_reset ();
	expr_reset ();
	dclpool_reset ();
	nsrecs = 0;
	ncases = 0;
	nbrk = ncont = 0;
	nloopst = 0;
	nsw = 0;
	nif = 0;
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
	struct srec *r;

	exproper (e);
	r = addrec (S_EXPR);
	r->r_e = e;
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
	struct srec *r;
	char *lt, *lf;

	lt = newlab ();
	lf = newlab ();
	iflab[nif][0] = lf;
	iflab[nif][1] = newlab ();		/* .Lend */
	++nif;

	r = addrec (S_BR);
	r->r_lab = lt;
	r->r_lf = lf;
	r->r_e = cond;
	addlab (lt);
}

void
sifend ()
{
	--nif;
	addlab (iflab[nif][0]);
}

void
selse ()
{
	addjump (iflab[nif - 1][1]);
	addlab (iflab[nif - 1][0]);
}

void
sifendelse ()
{
	--nif;
	addlab (iflab[nif][1]);
}

/*
 * while (c) A lowers to
 *	.Lc: B .Lb .Le c; .Lb: A; J .Lc; .Le:
 */
void
swhile (cond)
struct node *cond;
{
	struct srec *r;
	char *lc, *lb;

	lc = newlab ();
	lb = newlab ();
	brklab[nbrk++] = newlab ();		/* .Le */
	contlab[ncont++] = lc;

	addlab (lc);
	r = addrec (S_BR);
	r->r_lab = lb;
	r->r_lf = brklab[nbrk - 1];
	r->r_e = cond;
	addlab (lb);
}

void
swhileend ()
{
	addjump (contlab[ncont - 1]);
	addlab (brklab[--nbrk]);
	--ncont;
}

/*
 * do A while (c) lowers to
 *	.Lb: A; .Lc: B .Lb .Le c; .Le:
 */
void
sdo ()
{
	char *lb, *lc;

	lb = newlab ();
	lc = newlab ();
	brklab[nbrk++] = newlab ();		/* .Le */
	contlab[ncont++] = lc;
	loopst[nloopst].lab1 = lb;
	++nloopst;

	addlab (lb);
}

void
sdoend (cond)
struct node *cond;
{
	struct srec *r;

	--nloopst;
	addlab (contlab[ncont - 1]);
	r = addrec (S_BR);
	r->r_lab = loopst[nloopst].lab1;
	r->r_lf = brklab[nbrk - 1];
	r->r_e = cond;
	addlab (brklab[--nbrk]);
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
	struct srec *r;
	char *lc, *lb, *ls;

	if (e1 != NULL) {
		r = addrec (S_EXPR);
		r->r_e = e1;
	}
	lc = newlab ();
	lb = newlab ();
	ls = newlab ();
	brklab[nbrk++] = newlab ();		/* .Le */
	contlab[ncont++] = ls;
	loopst[nloopst].lab1 = ls;
	loopst[nloopst].lab2 = lc;
	loopst[nloopst].step = e3;
	++nloopst;

	addlab (lc);
	if (e2 != NULL) {
		r = addrec (S_BR);
		r->r_lab = lb;
		r->r_lf = brklab[nbrk - 1];
		r->r_e = e2;
	} else {
		addjump (lb);
	}
	addlab (lb);
}

void
sforend ()
{
	struct srec *r;

	--nloopst;
	addlab (loopst[nloopst].lab1);		/* .Ls */
	if (loopst[nloopst].step != NULL) {
		r = addrec (S_EXPR);
		r->r_e = loopst[nloopst].step;
	}
	addjump (loopst[nloopst].lab2);		/* .Lc */
	addlab (brklab[--nbrk]);
	--ncont;
}

/*
 * switch (c) stores c in a hidden local, then a chain of branches
 * compares it against the case constants; default and the end label
 * close the chain.  The chain is written when the function is
 * serialized, by which time all cases are known.
 */
void
sswitch (cond)
struct node *cond;
{
	struct srec *r;
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

	r = addrec (S_EXPR);
	r->r_e = nasgn ('=', t, cond);

	r = addrec (S_SW);
	r->r_t = t;
	r->r_lf = newlab ();			/* .Le */
	swst[nsw] = r;
	swtail[nsw] = &r->r_cases;
	swdflt[nsw] = NULL;
	swendlab[nsw] = r->r_lf;
	++nsw;
	brklab[nbrk++] = r->r_lf;
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
	*swtail[nsw - 1] = c;
	swtail[nsw - 1] = &c->next;

	addlab (c->lab);
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
	swdflt[nsw - 1] = newlab ();
	addlab (swdflt[nsw - 1]);
}

void
sswitchend ()
{
	addlab (swendlab[nsw - 1]);
	--nsw;
	--nbrk;
}

void
stmtret (e)
struct node *e;
{
	struct srec *r;
	struct type *rt;

	exproper (e);
	if (curfunc == NULL)
		return;
	rt = curfunc->s_tp->t_tp;
	if (e == NULL) {
		r = addrec (S_RET);
		if (isarith (rt) || isptr (rt)) {
			/* plain `return;' from a non-void function --
			   tolerated, as the C compiler did */
		}
		return;
	}
	if (!compat (rt, decay (e->n_tp)))
		typerr ("incompatible return value");
	r = addrec (S_RETV);
	r->r_e = e;
}

void
stmtgoto (name)
char *name;
{
	if (ngolist < NLAB)
		golist[ngolist++] = name;
	addjump (userlab (name));
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
	addlab (userlab (name));
}

void
stmtbrk ()
{
	if (loopdepth == 0 && swdepth == 0) {
		typerr ("break outside of loop or switch");
		return;
	}
	addjump (brklab[nbrk - 1]);
}

void
stmtcont ()
{
	if (loopdepth == 0) {
		typerr ("continue outside of loop");
		return;
	}
	addjump (contlab[ncont - 1]);
}
