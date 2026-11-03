#include "c0.h"

/*
 * type.c -- type construction and classification for c0.  Types are
 * small structures allocated from a pool and never freed; the common
 * base types are cached so that `int` is always the same pointer.
 */


static struct type	tpool[NTYPE];
static int		ntpool;
static struct type	*bcache[32];

int			farall;		/* -mcmodel=large */

struct type *
mktype (op, tp, size, memb, tag)
int op, size;
struct type *tp;
struct symb *memb;
char *tag;
{
	struct type *t;

	if (ntpool >= NTYPE)
		error ("too many types (%d used)", ntpool);
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
			typerr ("bad type combo");
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
		typerr ("bad type combo");
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

/* char types: fixbt canonicalizes them to BT_CHAR|BT_INT */
int
ischar (t)
struct type *t;
{
	return t != NULL && (t->t_op & BT_MASK & ~BT_INT) == BT_CHAR;
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
	return t != NULL && (t->t_op & ~T_FAR) == T_PTR;
}

int
isfarptr (t)
struct type *t;
{
	return t != NULL && (t->t_op & ~T_FAR) == T_PTR
	    && (t->t_op & T_FAR) != 0;
}

int
isscalar (t)
struct type *t;
{
	return isarith (t) || isptr (t);
}

/* arrays and functions decay to pointers when used in expressions;
   pointer types repeat heavily, so the last few are cached */
/*
 * Pointer types repeat heavily; they all come from here, cached on
 * the type they point at, so that expressions full of & and decay do
 * not exhaust the type pool.
 */
#define	NPTRTY	64

static struct type	*ptrcache[NPTRTY];
static int		 nptrty;
static struct type	*fptrcache[NPTRTY];
static int		 nfptrty;

static struct type *
mkptr (t, far)
struct type *t;
int far;
{
	struct type	**cache;
	int		*n;
	int		 i;

	cache = far ? fptrcache : ptrcache;
	n = far ? &nfptrty : &nptrty;
	for (i = 0; i < *n; ++i)
		if (cache[i]->t_tp == t)
			return cache[i];
	if (*n >= NPTRTY)		/* pathological: fall back */
		return mktype (T_PTR | (far ? T_FAR : 0), t, 0, NULL, NULL);
	cache[*n] = mktype (T_PTR | (far ? T_FAR : 0), t, 0, NULL, NULL);
	return cache[(*n)++];
}

struct type *
ptrtype (t)
struct type *t;
{
	/* In the large code model every data pointer is far.  Function
	   pointers stay near until far function pointers are supported. */
	if (farall && (t == NULL || t->t_op != T_FUNC))
		return mkptr (t, 1);
	return mkptr (t, 0);
}

struct type *
farptrtype (t)
struct type *t;
{
	return mkptr (t, 1);
}

/* arrays and functions decay to pointers when used in expressions */
struct type *
decay (t)
struct type *t;
{
	if (t == NULL)
		return NULL;
	if (t->t_op == T_ARY)
		return ptrtype (t->t_tp);	/* pointer to the element */
	if (t->t_op == T_FUNC)
		return ptrtype (t);
	return t;
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
	if (isscalar (a) && isscalar (b))
		return 1;		/* pointers mix freely with words */
	if (a->t_op == b->t_op && a->t_op == T_ARY)
		return 1;		/* extern T[] and T[n] get along */
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
		return (t->t_op & T_FAR) ? 4 : 2;
	case T_PTR | T_FAR:
		return 4;
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
