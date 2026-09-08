#include <string.h>
#include "c0.h"

/*
 * expr.c -- expression trees and type checking for c0.  Every
 * production of the expression grammar in c0.y calls one of the n*
 * builders below, which check its operands and return a node carrying
 * the result type.  Nodes come from a pool that is recycled once per
 * function definition.
 */


static struct node	npool[NNODE];
static int		nnpool;
long			lcons[NLCON];
static int		nlcon;

static struct node *
mknode (op, tp, l, r)
int op;
struct type *tp;
struct node *l, *r;
{
	struct node *n;

	if (nnpool >= NNODE)
		error ("expression too complex");
	n = &npool[nnpool++];
	n->n_op = op;
	n->n_tp = tp;
	n->n_l = l;
	n->n_r = r;
	n->n_val = 0;
	return n;
}

void
expr_reset ()
{
	nnpool = 0;
}

/*
 * A cast without an operand (`(type)`) is only legal as the operand of
 * sizeof; anywhere else it is an error.
 */
struct node *
exproper (e)
struct node *e;
{
	if (e != NULL && e->n_op == O_CAST && e->n_l == NULL) {
		typerr ("expected an expression after the cast");
		e->n_tp = btype (BT_INT);
	}
	return e;
}

/* ---------------- constant folding ---------------- */

/* signed 32/16 division and remainder, in ldivmod.asm; divisors must
   fit in a word, or the expression is not folded */
long	ldivmod ();

long
fold (e, ok)
struct node *e;
int *ok;
{
	long l, r;

	*ok = 1;
	if (e == NULL)
		return 0;
	if (e->n_op == O_CON)
		return e->n_val;
	if (e->n_op == O_LCON)
		return lcons[e->n_val];
	l = r = 0;
	if (e->n_l != NULL)
		l = fold (e->n_l, ok);
	if (*ok && e->n_r != NULL)
		r = fold (e->n_r, ok);
	if (!*ok)
		return 0;
	switch (e->n_op) {
	case O_CON:
		return e->n_val;
	case O_CAST:
		if (e->n_l == NULL) {
			*ok = 0;
			return 0;
		}
		return l;
	case '-':
		if (e->n_r == NULL)
			return -l;
		return l - r;
	case '+':
		if (e->n_r == NULL)
			return l;
		return l + r;
	case '*':
		return l * r;
	case '/':
		if (r == 0) {
			*ok = 0;
			return 0;
		}
		if (r >= -32768 && r <= 32767)
			return ldivmod (l, (int) r, 0);
		*ok = 0;
		return 0;
	case '%':
		if (r == 0) {
			*ok = 0;
			return 0;
		}
		if (r >= -32768 && r <= 32767)
			return ldivmod (l, (int) r, 1);
		*ok = 0;
		return 0;
	case '&':
		return l & r;
	case '|':
		return l | r;
	case '^':
		return l ^ r;
	case O_LS:
		return l << r;
	case O_RS:
		return l >> r;
	case O_LE:
		return l <= r;
	case O_GE:
		return l >= r;
	case '<':
		return l < r;
	case '>':
		return l > r;
	case O_EQ:
		return l == r;
	case O_NE:
		return l != r;
	case '~':
		return ~l;
	case '!':
		return !l;
	default:
		*ok = 0;
		return 0;
	}
}

/* ---------------- expressions ---------------- */

struct node *
nname (name)
char *name;
{
	struct symb *sp;
	struct node *n;

	sp = lookup (name);
	if (sp == NULL) {
		/*
		 * The parser may not have read the next token yet, so
		 * consult both its lookahead and the lexer's.
		 */
		if ((yychar >= 0 ? yychar : lexpeek ()) == '(') {
			/* implicit function declaration, as in K&R C;
			   always an external symbol */
			sp = ginstall (name, SC_EXTERN);
			sp->s_tp = mktype (T_FUNC, btype (BT_INT), 0, NULL,
					   NULL);
		} else {
			typerr ("'%s' undefined", name);
			sp = install (name, nscope > 0 ? SC_AUTO : SC_EXTERN);
			sp->s_tp = btype (BT_INT);
		}
	}
	n = mknode (O_NAME, sp->s_tp, (struct node *) sp, NULL);
	return n;
}

struct node *
ncon ()
{
	struct node *n;

	n = mknode (O_CON, btype (numbt), NULL, NULL);
	if (numval < -32768 || numval > 32767) {
		if (nlcon >= NLCON)
			error ("too many long constants");
		lcons[nlcon] = numval;
		n->n_op = O_LCON;
		n->n_val = nlcon++;
	} else
		n->n_val = (int) numval;
	return n;
}

struct node *
nstr (idx)
int idx;
{
	struct node *n;

	n = mknode (O_STR, mktype (T_PTR, btype (BT_CHAR), 0, NULL, NULL),
		    NULL, NULL);
	n->n_val = idx;
	return n;
}

static void
bscalars (l, r)
struct node *l, *r;
{
	exproper (l);
	exproper (r);
}

struct node *
nbina (op, l, r)
int op;
struct node *l, *r;
{
	struct type *lt, *rt;

	bscalars (l, r);
	lt = decay (l->n_tp);
	rt = decay (r->n_tp);
	if (lt == NULL || rt == NULL)
		return mknode (op, btype (BT_INT), l, r);

	switch (op) {
	case '+':
		if (isptr (lt) && isarith (rt))
			return mknode (op, lt, l, r);
		if (isarith (lt) && isptr (rt))
			return mknode (op, rt, l, r);
		if (isarith (lt) && isarith (rt))
			return mknode (op, usual (lt, rt), l, r);
		break;
	case '-':
		if (isptr (lt) && isarith (rt))
			return mknode (op, lt, l, r);
		if (isptr (lt) && isptr (rt))
			return mknode (op, btype (BT_INT), l, r);
		if (isarith (lt) && isarith (rt))
			return mknode (op, usual (lt, rt), l, r);
		break;
	case '*':
	case '/':
	case '%':
	case '&':
	case '|':
	case '^':
	case O_LS:
	case O_RS:
		if (isarith (lt) && isarith (rt)) {
			if (op == O_LS || op == O_RS)
				return mknode (op, lt, l, r);
			return mknode (op, usual (lt, rt), l, r);
		}
		break;
	case '<':
	case '>':
	case O_LE:
	case O_GE:
	case O_EQ:
	case O_NE:
		if (isscalar (lt) && isscalar (rt))
			return mknode (op, btype (BT_INT), l, r);
		break;
	}
	typerr ("invalid operands to binary operator");
	return mknode (op, btype (BT_INT), l, r);
}

struct node *
nlog (op, l, r)
int op;
struct node *l, *r;
{
	bscalars (l, r);
	if (!isscalar (decay (l->n_tp)) || !isscalar (decay (r->n_tp)))
		typerr ("invalid operands to %s",
			op == ANDAND ? "&&" : "||");
	return mknode (op, btype (BT_INT), l, r);
}

static int
islval (e)
struct node *e;
{
	struct type *t = e->n_tp;

	switch (e->n_op) {
	case O_NAME:
	case O_INDEX:
	case O_MEMBER:
	case O_ARROW:
	case '*':
		break;
	default:
		return 0;
	}
	if (t == NULL)
		return 0;
	return t->t_op != T_FUNC && t->t_op != T_ARY;
}

struct node *
nasgn (op, l, r)
int op;
struct node *l, *r;
{
	struct type *lt, *rt;
	int baseop;

	exproper (r);
	if (!islval (l)) {
		typerr ("assignment to non-lvalue");
		return mknode (op, l->n_tp, l, r);
	}
	lt = l->n_tp;
	rt = decay (r->n_tp);
	if (lt->t_op == T_ARY || lt->t_op == T_FUNC) {
		typerr ("cannot assign to '%s'",
			lt->t_op == T_ARY ? "array" : "function");
		return mknode ('=', lt, l, r);
	}
	if (!compat (lt, rt)) {
		typerr ("incompatible assignment");
	} else if (op != '=') {
		/* compound assignment: check the base operation */
		baseop = op == O_ADDA ? '+'
		      : op == O_SUBA ? '-'
		      : op == O_MULA ? '*'
		      : op == O_DIVA ? '/'
		      : op == O_MODA ? '%'
		      : op == O_ANDA ? '&'
		      : op == O_ORA ? '|'
		      : op == O_XORA ? '^'
		      : op == O_LSA ? O_LS : O_RS;
		if (isptr (decay (lt)) && baseop != '+' && baseop != '-')
			typerr ("invalid pointer assignment");
		else if (!isptr (decay (lt)) && !isarith (decay (lt)))
			typerr ("invalid assignment operand");
	}
	/* keep the compound operator in n_op for the IR emitter */
	return mknode (op, lt, l, r);
}

struct node *
ncond (c, t, f)
struct node *c, *t, *f;
{
	struct node *n;

	exproper (c);
	exproper (t);
	exproper (f);
	if (!isscalar (decay (c->n_tp)))
		typerr ("controlling expression must be scalar");
	if (t != NULL && f != NULL && !compat (decay (t->n_tp),
					       decay (f->n_tp)))
		typerr ("incompatible branches of ?:");
	n = mknode (O_COND, t == NULL ? btype (BT_INT) : t->n_tp, c, t);
	n->n_val = (long) (int) f;		/* third child, kept in n_val */
	return n;
}

struct node *
nun (op, e)
int op;
struct node *e;
{
	struct type *t;

	exproper (e);
	t = decay (e->n_tp);
	if (t == NULL)
		return mknode (op, btype (BT_INT), e, NULL);

	switch (op) {
	case '*':
		if (t->t_op != T_PTR) {
			typerr ("cannot dereference a non-pointer");
			return mknode (op, btype (BT_INT), e, NULL);
		}
		if ((t->t_tp->t_op == T_STRUCT || t->t_tp->t_op == T_UNION)
		    && t->t_tp->t_memb == NULL)
			typerr ("dereference of pointer to incomplete type");
		return mknode (op, t->t_tp, e, NULL);
	case '+':
	case '-':
		if (!isarith (t))
			typerr ("invalid operand to unary %c", op);
		return mknode (op, e->n_tp, e, NULL);
	case '!':
		if (!isscalar (t))
			typerr ("invalid operand to !");
		return mknode (op, btype (BT_INT), e, NULL);
	case '~':
		if (!isarith (t))
			typerr ("invalid operand to ~");
		return mknode (op, btype (BT_INT), e, NULL);
	}
	typerr ("invalid unary operator");
	return mknode (op, btype (BT_INT), e, NULL);
}

struct node *
naddr (e)
struct node *e;
{
	struct type *t;

	if (e == NULL) {
		return mknode (O_ADDR, btype (BT_INT), NULL, NULL);
	}
	t = e->n_tp;
	if (e->n_op == O_NAME && t != NULL && t->t_op == T_FUNC)
		return mknode (O_ADDR, mktype (T_PTR, t, 0, NULL, NULL),
			       e, NULL);
	if (!islval (e)) {
		typerr ("cannot take the address of this expression");
		return mknode (O_ADDR, btype (BT_INT), e, NULL);
	}
	if (t->t_op == T_ARY)		/* &array: pointer to element */
		t = t->t_tp;
	return mknode (O_ADDR, mktype (T_PTR, t, 0, NULL, NULL), e, NULL);
}

struct node *
ninc (op, e, pre)
int op, pre;
struct node *e;
{
	exproper (e);
	if (!islval (e))
		typerr ("operand of ++/-- must be an lvalue");
	else if (!isscalar (decay (e->n_tp)))
		typerr ("operand of ++/-- must be scalar");
	if (pre)
		return mknode (op == '+' ? O_PREINC : O_PREDEC, e->n_tp, e,
			       NULL);
	return mknode (op == '+' ? O_POSTINC : O_POSTDEC, e->n_tp, e,
		       NULL);
}

struct node *
nindex (l, r)
struct node *l, *r;
{
	struct type *lt, *rt;

	exproper (l);
	exproper (r);
	lt = decay (l->n_tp);
	rt = decay (r->n_tp);
	if (lt == NULL || rt == NULL)
		return mknode (O_INDEX, btype (BT_INT), l, r);
	if (isptr (lt) && isarith (rt))
		return mknode (O_INDEX, lt->t_tp, l, r);
	if (isarith (lt) && isptr (rt))
		return mknode (O_INDEX, rt->t_tp, l, r);
	typerr ("invalid operands to []");
	return mknode (O_INDEX, btype (BT_INT), l, r);
}

struct node *
nmember (l, op, name)
struct node *l;
int op;
char *name;
{
	struct type *t;
	struct symb *m;

	exproper (l);
	t = l->n_tp;
	if (op == O_ARROW) {
		t = decay (t);
		if (t == NULL || t->t_op != T_PTR) {
			typerr ("left of -> is not a pointer");
			return mknode (op, btype (BT_INT), l, NULL);
		}
		t = t->t_tp;
	}
	if (t == NULL || (t->t_op != T_STRUCT && t->t_op != T_UNION)) {
		typerr ("left of %s is not a struct or union",
			op == O_ARROW ? "->" : ".");
		return mknode (op, btype (BT_INT), l, NULL);
	}
	if (t->t_memb == NULL) {
		typerr ("member access to incomplete type");
		return mknode (op, btype (BT_INT), l, NULL);
	}
	for (m = t->t_memb; m != NULL; m = m->s_next)
		if (strcmp (m->s_name, name) == 0)
			break;
	if (m == NULL) {
		typerr ("no member named '%s'", name);
		return mknode (op, btype (BT_INT), l, NULL);
	}
	/* the member symbol rides in n_r, so the emitter can find its
	   offset */
	return mknode (op, m->s_tp, l, (struct node *) m);
}

static int
argcount (e)
struct node *e;
{
	if (e == NULL)
		return 0;
	if (e->n_op != O_COMMA)
		return 1;
	return argcount (e->n_l) + argcount (e->n_r);
}

struct node *
ncall (f, args)
struct node *f, *args;
{
	struct node *a;
	struct type *ft, *rt;
	struct symb *sp;
	int n, want;

	exproper (f);
	ft = f->n_tp;
	if (ft != NULL && ft->t_op == T_FUNC)
		;
	else if ((ft = decay (ft)) != NULL && ft->t_op == T_PTR
		 && ft->t_tp->t_op == T_FUNC)
		ft = ft->t_tp;
	else {
		typerr ("called object is not a function");
		return mknode (O_CALL, btype (BT_INT), f, args);
	}
	rt = ft->t_tp;

	n = argcount (args);
	if (f->n_op == O_NAME) {
		sp = (struct symb *) f->n_l;
		if (sp->s_tp->t_op == T_FUNC && sp->s_tp->t_memb != NULL) {
			want = 0;
			for (sp = sp->s_tp->t_memb; sp != NULL;
			     sp = sp->s_next)
				++want;
			if (n != want)
				typerr ("wrong number of arguments (want %d, got %d)",
					want, n);
		}
	}
	for (a = args; a != NULL; ) {
		exproper (a);
		if (a->n_op == O_COMMA) {
			exproper (a->n_l);
			a = a->n_r;
		} else
			a = NULL;
	}
	return mknode (O_CALL, rt, f, args);
}

struct node *
ncast (tp, e)
struct type *tp;
struct node *e;
{
	if (e == NULL)
		return mknode (O_CAST, tp, NULL, NULL);
	exproper (e);
	if ((tp->t_op < 0x100 || isptr (tp)) && !isscalar (decay (e->n_tp))) {
		/* casting a struct to a scalar: unusual, but tolerate */
	} else if (tp->t_op == T_STRUCT || tp->t_op == T_UNION)
		typerr ("cannot cast to struct or union");
	return mknode (O_CAST, tp, e, NULL);
}

struct node *
ncomma (l, r)
struct node *l, *r;
{
	exproper (l);
	exproper (r);
	return mknode (O_COMMA, r == NULL ? btype (BT_INT) : r->n_tp, l, r);
}

struct node *
nsize (e, tp)
struct node *e;
struct type *tp;
{
	struct node *n;
	int sz;

	if (tp != NULL)
		sz = tysize (tp);
	else if (e != NULL && e->n_op == O_CAST && e->n_l == NULL)
		sz = tysize (e->n_tp);
	else {
		exproper (e);
		if (e->n_tp == NULL || e->n_tp->t_op == T_FUNC) {
			typerr ("invalid operand to sizeof");
			sz = 2;
		} else
			sz = tysize (e->n_tp);
	}
	n = mknode (O_CON, btype (BT_INT | BT_UNSIGNED), NULL, NULL);
	n->n_val = sz;
	return n;
}

struct node *
ilist (list)
struct node *list;
{
	return mknode (O_ILIST, NULL, list, NULL);
}

/* a constant of the given value (word or long, whichever fits) */
struct node *
nconst (v)
long v;
{
	struct node *n;

	n = mknode (O_CON, btype (BT_INT), NULL, NULL);
	if (v < -32768 || v > 32767) {
		if (nlcon >= NLCON)
			error ("too many long constants");
		lcons[nlcon] = v;
		n->n_op = O_LCON;
		n->n_val = nlcon++;
	} else
		n->n_val = (int) v;
	return n;
}

/* a reference node for a symbol the parser never saw (switch temps) */
struct node *
nlocal (sp)
struct symb *sp;
{
	return mknode (O_NAME, sp->s_tp, (struct node *) sp, NULL);
}
