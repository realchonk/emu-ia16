#include <string.h>
#include <stdio.h>
#include "c0.h"

/*
 * stmt.c -- function definitions and statement checking for c0.  A
 * function definition installs its symbol and parameters (fdef_dcl),
 * the statement actions then check expressions and labels as the body
 * is parsed, and fdefend closes the function: labels named by goto
 * must exist, and the temporary pools are recycled.
 */

#define NLAB	100

static struct symb	*curfunc;
static char		*labels[NLAB];
static int		nlabels;
static char		*golist[NLAB];
static int		ngolist;
static int		loopdepth, swdepth;

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
	int i, j;

	for (i = 0; i < ngolist; ++i) {
		for (j = 0; j < nlabels; ++j)
			if (strcmp (golist[i], labels[j]) == 0)
				break;
		if (j == nlabels)
			typerr ("undefined label '%s'", golist[i]);
	}
	blkpop ();
	curfunc = NULL;
	dcl_reset ();
	expr_reset ();
	dclpool_reset ();
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
	exproper (e);
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

void
stmtcase (e)
struct node *e;
{
	int ok;

	if (swdepth == 0)
		typerr ("case outside of switch");
	if (e != NULL) {
		fold (exproper (e), &ok);
		if (!ok)
			typerr ("case label is not constant");
	}
}

void
stmtdflt ()
{
	if (swdepth == 0)
		typerr ("default outside of switch");
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
		if (isarith (rt) || isptr (rt)) {
			/* plain `return;' from a non-void function --
			   tolerated, as the C compiler did */
		}
		return;
	}
	if (!compat (rt, decay (e->n_tp)))
		typerr ("incompatible return value");
}

void
stmtgoto (name)
char *name;
{
	if (ngolist < NLAB)
		golist[ngolist++] = name;
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
}

void
stmtbrk ()
{
	if (loopdepth == 0 && swdepth == 0)
		typerr ("break outside of loop or switch");
}

void
stmtcont ()
{
	if (loopdepth == 0)
		typerr ("continue outside of loop");
}
