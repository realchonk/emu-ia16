#include "c0.h"

/*
 * type.c -- type construction and classification for c0.  Types are
 * small structures allocated from a pool and never freed; the common
 * base types are cached so that `int` is always the same pointer.
 */


static struct type	tpool[NTYPE];
static int		ntpool;
static struct type	*bcache[32];

struct type *
mktype (op, tp, size, memb, tag)
int op, size;
struct type *tp;
struct symb *memb;
char *tag;
{
	struct type *t;

	if (ntpool >= NTYPE)
		error ("too many types");
	t = &tpool[ntpool++];
	t->t_op = op;
	t->t_tp = tp;
	t->t_size = size;
	t->t_memb = memb;
	t->t_tag = tag;
	return t;
}

/*
 * Validate a combination of base type bits and return the canonical
 * form: implicit int is filled in.
 */
int
fixbt (bt)
int bt;
{
	int base = bt & (BT_CHAR | BT_SHORT | BT_INT | BT_LONG);

	if (bt & BT_VOID) {
		if (base != BT_NONE || bt & (BT_UNSIGNED | BT_SIGNED)) {
			typerr ("invalid type combination");
			return BT_INT;
		}
		return bt;
	}
	switch (base) {
	case BT_NONE:
	case BT_INT:
	case BT_SHORT:
	case BT_LONG:
	case BT_CHAR:
		break;
	default:
		typerr ("invalid type combination");
		return BT_INT;
	}
	return bt | BT_INT;
}

struct type *
btype (bt)
int bt;
{
	if (bt >= 0 && bt < 32) {
		if (bcache[bt] == NULL)
			bcache[bt] = mktype (bt, NULL, 0, NULL, NULL);
		return bcache[bt];
	}
	return mktype (bt, NULL, 0, NULL, NULL);
}

int
isarith (t)
struct type *t;
{
	return t != NULL && t->t_op < 0x100 && !(t->t_op & BT_VOID);
}

int
isptr (t)
struct type *t;
{
	return t != NULL && t->t_op == T_PTR;
}

int
isscalar (t)
struct type *t;
{
	return isarith (t) || isptr (t);
}

/* arrays and functions decay to pointers when used in expressions;
   pointer types repeat heavily, so the last few are cached */
/* arrays and functions decay to pointers when used in expressions;
   the resulting pointer types repeat heavily, so they are cached */
struct type *
decay (t)
struct type *t;
{
	static struct type *cache[24];
	int i;

	if (t == NULL)
		return NULL;
	if (t->t_op == T_ARY)
		t = t->t_tp;		/* pointer to the element */
	else if (t->t_op != T_FUNC)
		return t;
	for (i = 0; i < 24 && cache[i] != NULL; ++i)
		if (cache[i]->t_tp == t)
			return cache[i];
	if (i == 24)
		return mktype (T_PTR, t, 0, NULL, NULL);
	cache[i] = mktype (T_PTR, t, 0, NULL, NULL);
	return cache[i];
}

/* the usual arithmetic conversions, kept K&R-simple */
struct type *
usual (a, b)
struct type *a, *b;
{
	int uns = (a->t_op | b->t_op) & BT_UNSIGNED;

	if ((a->t_op | b->t_op) & BT_LONG)
		return btype (BT_LONG | uns);
	return btype (BT_INT | uns);
}

/* K&R-lenient type compatibility */
int
compat (a, b)
struct type *a, *b;
{
	if (a == NULL || b == NULL)
		return 1;		/* an error was already reported */
	if (a == b)
		return 1;
	if (isarith (a) && isarith (b))
		return 1;
	if (isptr (a) && isptr (b))
		return 1;
	if (isptr (a) && isarith (b))
		return 1;
	if (isarith (a) && isptr (b))
		return 1;
	if (a->t_op == b->t_op
	    && (a->t_op == T_STRUCT || a->t_op == T_UNION))
		return a->t_memb != NULL && a->t_memb == b->t_memb;
	return 0;
}

int
tysize (t)
struct type *t;
{
	int op;

	if (t == NULL)
		return 0;
	switch (t->t_op) {
	case T_PTR:
	case T_FUNC:
		return 2;
	case T_ARY:
		return t->t_size * tysize (t->t_tp);
	case T_STRUCT:
	case T_UNION:
		return t->t_size;
	}
	/* the longer types win; fixbt ors BT_INT in implicitly */
	op = t->t_op & BT_MASK;
	if (op & BT_LONG)
		return 4;
	if (op & BT_SHORT)
		return 2;
	if (op & BT_CHAR)
		return 1;
	return 2;
}
