#include <string.h>
#include <stdio.h>
#include "c0.h"

/*
 * decl.c -- declarations for c0: the symbol table with block scopes,
 * declaration specifiers, struct and union definitions, and the
 * declarator machinery that turns `int *a[3]` into a type.
 */

#define NSYMB	768
#define NSCOPE	16
#define NSU	16
#define NDCL	256
#define NDSPEC	96

/* ---------------- symbol table ---------------- */

static struct symb	spool[NSYMB];
static int		nspool;
static struct symb	*sfreelist;

static struct symb	*scopes[NSCOPE];	/* scopes[1..nscope] */
int			nscope;
static struct symb	*globals;
static struct symb	*tags;

static struct symb *
mksymb ()
{
	struct symb *s;

	if (sfreelist != NULL) {
		s = sfreelist;
		sfreelist = s->s_next;
	} else {
		if (nspool >= NSYMB)
			error ("too many symbols");
		s = &spool[nspool++];
	}
	s->s_next = NULL;
	s->s_name = NULL;
	s->s_tp = NULL;
	s->s_sc = 0;
	return s;
}

static struct symb *
scopefind (chain, name)
struct symb *chain;
char *name;
{
	for (; chain != NULL; chain = chain->s_next)
		if (strcmp (chain->s_name, name) == 0)
			return chain;
	return NULL;
}

struct symb *
lookup (name)
char *name;
{
	struct symb *s;
	int i;

	for (i = nscope; i > 0; --i)
		if ((s = scopefind (scopes[i], name)) != NULL)
			return s;
	return scopefind (globals, name);
}

struct symb *
install (name, sc)
char *name;
int sc;
{
	struct symb *s;

	s = mksymb ();
	s->s_name = name;
	s->s_sc = sc;
	if (nscope > 0) {
		s->s_next = scopes[nscope];
		scopes[nscope] = s;
	} else {
		s->s_next = globals;
		globals = s;
	}
	return s;
}

void
blkpush ()
{
	if (++nscope >= NSCOPE)
		error ("blocks nested too deeply");
	scopes[nscope] = NULL;
}

void
blkpop ()
{
	struct symb *s, *next;

	if (nscope <= 0)
		return;
	for (s = scopes[nscope]; s != NULL; s = next) {
		next = s->s_next;
		s->s_next = sfreelist;
		sfreelist = s;
	}
	scopes[nscope] = NULL;
	--nscope;
}

/*
 * Install a function parameter (default int) in the current scope.
 * Returns NULL if the name is already taken.
 */
struct symb *
insparam (name)
char *name;
{
	struct symb *sp;

	if (scopefind (scopes[nscope], name) != NULL)
		return NULL;
	sp = install (name, SC_PARAM);
	sp->s_tp = btype (BT_INT);
	return sp;
}

/* ---------------- declaration specifiers ---------------- */

static struct {
	int		 sc;
	int		 bt;
	struct type	*tp;
} curd;

void
dcl_reset ()
{
	curd.sc = 0;
	curd.bt = 0;
	curd.tp = NULL;
}

void
sc_sclass (sc)
int sc;
{
	if (curd.sc != 0)
		typerr ("multiple storage classes");
	curd.sc = sc;
}

void
sc_type (bt)
int bt;
{
	if (curd.tp != NULL)
		typerr ("too many types");
	curd.bt |= bt;
}

void
sc_const ()
{
	curd.bt |= BT_CONST;
}

void
sc_su (tp)
struct type *tp;
{
	if (curd.tp != NULL)
		typerr ("too many types");
	curd.tp = tp;
}

void
sc_td (sp)
struct symb *sp;
{
	if (curd.tp != NULL)
		typerr ("too many types");
	curd.tp = sp->s_tp;
}

/* the base type accumulated so far */
struct type *
curbase ()
{
	if (curd.tp != NULL)
		return curd.tp;
	return btype (fixbt (curd.bt));
}

/* the storage class accumulated so far */
int
curd_sc ()
{
	return curd.sc;
}

/* ---------------- struct and union declarations ---------------- */

static struct {
	struct type	*su;
	struct symb	**tail;
	int		 size;
	int		 isunion;
} sustack[NSU];
static int nsu;

static struct type *
tagfind (name)
char *name;
{
	struct symb *t;

	for (t = tags; t != NULL; t = t->s_next)
		if (strcmp (t->s_name, name) == 0)
			return t->s_tp;
	return NULL;
}

static struct type *
tagtype (sou, name)
int sou;
char *name;
{
	struct type *tp;
	struct symb *t;

	if ((tp = tagfind (name)) != NULL)
		return tp;
	tp = mktype (sou, NULL, 0, NULL, name);
	t = mksymb ();
	t->s_name = name;
	t->s_sc = SC_TAG;
	t->s_tp = tp;
	t->s_next = tags;
	tags = t;
	return tp;
}

struct type *
su_begin (sou, tag)
int sou;
char *tag;
{
	struct type *tp;

	if (tag != NULL) {
		tp = tagtype (sou, tag);
		if (tp->t_memb != NULL)
			typerr ("redefinition of '%s'", tag);
		tp->t_op = sou;
	} else
		tp = mktype (sou, NULL, 0, NULL, NULL);

	if (nsu >= NSU)
		error ("structs nested too deeply");
	sustack[nsu].su = tp;
	sustack[nsu].tail = &tp->t_memb;
	sustack[nsu].size = 0;
	sustack[nsu].isunion = (sou == T_UNION);
	++nsu;
	return tp;
}

struct type *
su_end ()
{
	struct type *tp;

	if (nsu == 0)
		error ("internal: struct stack underflow");
	--nsu;
	tp = sustack[nsu].su;
	tp->t_size = sustack[nsu].size;
	return tp;
}

struct type *
su_ref (sou, tag)
int sou;
char *tag;
{
	struct type *tp;

	tp = tagtype (sou, tag);
	tp->t_op = sou;
	return tp;
}

void
member (d)
struct dcl *d;
{
	struct type *tp = dcltype (curbase (), d);
	struct symb *m, **tail;
	char *name = dclname (d);
	int sz, align;

	if (nsu == 0) {
		typerr ("member outside struct");
		return;
	}
	if (name == NULL) {
		typerr ("member name omitted");
		return;
	}
	if (tp->t_op == T_FUNC) {
		typerr ("'%s' has function type", name);
		return;
	}
	if ((tp->t_op == T_STRUCT || tp->t_op == T_UNION)
	    && tp->t_memb == NULL) {
		typerr ("member '%s' has incomplete type", name);
		return;
	}
	if (scopefind (sustack[nsu - 1].su->t_memb, name) != NULL) {
		typerr ("duplicate member '%s'", name);
		return;
	}

	m = mksymb ();
	m->s_name = name;
	m->s_tp = tp;
	tail = sustack[nsu - 1].tail;
	*tail = m;
	sustack[nsu - 1].tail = &m->s_next;

	sz = tysize (tp);
	if (sustack[nsu - 1].isunion) {
		if (sz > sustack[nsu - 1].size)
			sustack[nsu - 1].size = sz;
	} else {
		align = sz > 1 ? 2 : 1;
		sustack[nsu - 1].size =
			(sustack[nsu - 1].size + align - 1) & ~(align - 1);
		sustack[nsu - 1].size += sz;
	}
}

/* ---------------- declarator parse trees ---------------- */

static struct dcl	dpool[NDCL];
static int		ndpool;

static struct dcl *
mkdcl (op)
int op;
{
	struct dcl *d;

	if (ndpool >= NDCL)
		error ("declarator too complex");
	d = &dpool[ndpool++];
	d->d_op = op;
	d->d_l = NULL;
	d->d_size = NULL;
	d->d_name = NULL;
	d->d_params = NULL;
	return d;
}

struct dcl *
dstar (op)
int op;
{
	if (op != '*')
		typerr ("expected '*' in declarator");
	return mkdcl (D_PTR);
}

struct dcl *
dptrn (d, op)
struct dcl *d;
int op;
{
	struct dcl *p;

	if (op != '*')
		typerr ("expected '*' in declarator");
	p = mkdcl (D_PTR);
	p->d_l = d;
	return p;
}

struct dcl *
dchain (ptrs, d)
struct dcl *ptrs, *d;
{
	struct dcl *p = ptrs;

	while (p->d_l != NULL)
		p = p->d_l;
	p->d_l = d;
	return ptrs;
}

struct dcl *
dname (name)
char *name;
{
	struct dcl *d = mkdcl (D_NAME);

	d->d_name = name;
	return d;
}

struct dcl *
dfunc (d, params)
struct dcl *d;
struct symb *params;
{
	struct dcl *f = mkdcl (D_FUNC);

	f->d_l = d;
	f->d_params = params;
	return f;
}

struct dcl *
dary (d, size)
struct dcl *d;
struct node *size;
{
	struct dcl *a = mkdcl (D_ARY);

	a->d_l = d;
	a->d_size = size;
	return a;
}

struct symb *
param1 (name)
char *name;
{
	struct symb *p = mksymb ();

	p->s_name = name;
	return p;
}

struct symb *
paramn (list, name)
struct symb *list;
char *name;
{
	struct symb *p = mksymb ();

	p->s_name = name;
	p->s_next = list;
	return p;
}

/*
 * Interpret a declarator tree: walking down from the root, each node
 * wraps the base type (a pointer outside the name, an array or a
 * function inside it), so `int *a[3]` yields array-of-pointer and
 * `int (*f)()` yields pointer-to-function.
 */
struct type *
dcltype (t, d)
struct type *t;
struct dcl *d;
{
	int ok, n;
	long v;

	while (d != NULL) {
		switch (d->d_op) {
		case D_NAME:
			return t;
		case D_PTR:
			t = mktype (T_PTR, t, 0, NULL, NULL);
			break;
		case D_ARY:
			n = 0;
			if (d->d_size != NULL) {
				v = fold (d->d_size, &ok);
				if (!ok)
					typerr ("array size is not constant");
				else
					n = (int) v;
			}
			t = mktype (T_ARY, t, n, NULL, NULL);
			break;
		case D_FUNC:
			t = mktype (T_FUNC, t, 0, d->d_params, NULL);
			break;
		default:
			error ("internal: bad declarator");
		}
		d = d->d_l;
	}
	return t;
}

char *
dclname (d)
struct dcl *d;
{
	while (d != NULL && d->d_op != D_NAME)
		d = d->d_l;
	return d == NULL ? NULL : d->d_name;
}

/* ---------------- type names (casts, sizeof) ---------------- */

static struct dspec	dspool[NDSPEC];
static int		ndspool;

static struct dspec *
mkdspec ()
{
	if (ndspool >= NDSPEC)
		error ("too many type names");
	return &dspool[ndspool++];
}

void
dclpool_reset ()
{
	ndpool = 0;
	ndspool = 0;
}

struct dspec *
tn_bt (bt)
int bt;
{
	struct dspec *p = mkdspec ();

	p->p_sc = 0;
	p->p_bt = bt;
	p->p_tp = NULL;
	return p;
}

struct dspec *
tn_td (sp)
struct symb *sp;
{
	struct dspec *p = mkdspec ();

	p->p_sc = 0;
	p->p_bt = 0;
	p->p_tp = sp->s_tp;
	return p;
}

struct dspec *
tn_su (tp)
struct type *tp;
{
	struct dspec *p = mkdspec ();

	p->p_sc = 0;
	p->p_bt = 0;
	p->p_tp = tp;
	return p;
}

struct dspec *
tn_cat (a, b)
struct dspec *a, *b;
{
	if (a->p_tp != NULL && b->p_tp != NULL)
		typerr ("too many types");
	if (a->p_tp == NULL)
		a->p_tp = b->p_tp;
	a->p_bt |= b->p_bt;
	return a;
}

struct type *
tn_type (sp, abs)
struct dspec *sp;
struct dcl *abs;
{
	struct type *base;

	if (sp->p_tp != NULL)
		base = sp->p_tp;
	else
		base = btype (fixbt (sp->p_bt));
	return dcltype (base, abs);
}

/* ---------------- declarations ---------------- */

void
bindparam (d)
struct dcl *d;
{
	struct type *tp = dcltype (curbase (), d);
	char *name = dclname (d);
	struct symb *p;

	if (name == NULL) {
		typerr ("parameter name omitted");
		return;
	}
	p = lookup (name);
	if (p == NULL || p->s_sc != SC_PARAM) {
		typerr ("'%s' is not a parameter", name);
		return;
	}
	p->s_tp = tp;
}

void
dclinst (d, init)
struct dcl *d;
struct node *init;
{
	struct type *tp = dcltype (curbase (), d);
	struct symb *sp, *old;
	char *name = dclname (d);
	int sc;

	if (curd.sc == SC_TYPEDEF) {
		if (name == NULL) {
			typerr ("typedef name omitted");
			return;
		}
		if (lookup (name) != NULL) {
			typerr ("redeclaration of '%s'", name);
			return;
		}
		sp = install (name, SC_TYPEDEF);
		sp->s_tp = tp;
		return;
	}

	if (name == NULL) {
		typerr ("name omitted in declarator");
		return;
	}
	if (tp->t_op == T_FUNC) {
		/* declared function: redeclarations are fine */
		old = scopefind (globals, name);
		if (old != NULL && nscope == 0) {
			if (old->s_tp->t_op != T_FUNC)
				typerr ("redeclaration of '%s'", name);
			else if (!compat (old->s_tp->t_tp, tp->t_tp))
				typerr ("conflicting return type for '%s'",
					name);
			old->s_tp = tp;
			chkinit (tp, init);
			return;
		}
		if (old != NULL && nscope > 0) {
			typerr ("redeclaration of '%s'", name);
			return;
		}
		sc = curd.sc == 0 ? SC_EXTERN : curd.sc;
	} else {
		old = nscope > 0 ? scopefind (scopes[nscope], name)
				 : scopefind (globals, name);
		if (old != NULL) {
			if (nscope > 0 || !compat (old->s_tp, tp)
			    || old->s_sc == SC_TYPEDEF)
				typerr ("redeclaration of '%s'", name);
			else
				old->s_tp = tp;
			chkinit (tp, init);
			return;
		}
		sc = curd.sc == 0 ? (nscope > 0 ? SC_AUTO : SC_EXTERN)
				  : curd.sc;
	}

	sp = install (name, sc);
	sp->s_tp = tp;
	if ((tp->t_op == T_STRUCT || tp->t_op == T_UNION)
	    && tp->t_memb == NULL)
		typerr ("'%s' has incomplete type", name);
	else if (tp->t_op == (BT_VOID | BT_CONST)
		 || (tp->t_op & BT_MASK) == BT_VOID)
		typerr ("'%s' has void type", name);
	chkinit (tp, init);
}

void
chkinit (tp, init)
struct type *tp;
struct node *init;
{
	struct node *list, *item;
	struct symb *m;
	int n, count;

	if (init == NULL)
		return;
	if (init->n_op == O_ILIST) {
		list = init->n_l;
		if (tp->t_op == T_ARY) {
			if (list != NULL && list->n_op == O_STR
			    && (tp->t_tp->t_op & BT_MASK) == BT_CHAR)
				return;
			count = 0;
			for (item = list; item != NULL; ) {
				++count;
				item = item->n_op == O_COMMA ? item->n_r : NULL;
			}
			if (tp->t_size != 0 && count > tp->t_size)
				typerr ("too many initializers");
			for (item = list; item != NULL; ) {
				if (!compat (tp->t_tp,
					     decay (item->n_tp)))
					typerr ("bad initializer element");
				item = item->n_op == O_COMMA ? item->n_r
							     : NULL;
			}
		} else if (tp->t_op == T_STRUCT || tp->t_op == T_UNION) {
			n = 0;
			for (m = tp->t_memb; m != NULL; m = m->s_next)
				++n;
			count = 0;
			for (item = list; item != NULL; ) {
				++count;
				item = item->n_op == O_COMMA ? item->n_r : NULL;
			}
			if (count > n)
				typerr ("too many initializers");
		}
		/* scalars take `{ expr }` as in K&R C */
		return;
	}

	if (tp->t_op == T_ARY) {
		if (init->n_op == O_STR
		    && (tp->t_tp->t_op & BT_MASK) == BT_CHAR)
			return;
		typerr ("array initializer requires braces");
		return;
	}
	if (!compat (tp, decay (init->n_tp)))
		typerr ("incompatible initializer");
}
