#include <string.h>
#include <unistd.h>
#include "c0.h"

/*
 * decl.c -- declarations for c0: the symbol table with block scopes,
 * declaration specifiers, struct and union definitions, and the
 * declarator machinery that turns `int *a[3]` into a type.
 */


static void	chkitem ();

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
	s->s_name = 0;
	s->s_tp = NULL;
	s->s_sc = 0;
	return s;
}

static struct symb *
scopefind (chain, name)
struct symb *chain;
int name;
{
	for (; chain != NULL; chain = chain->s_next)
		if (chain->s_name == name)
			return chain;
	return NULL;
}

struct symb *
lookup (name)
int name;
{
	struct symb *s;
	int i;

	for (i = nscope; i > 0; --i)
		if ((s = scopefind (scopes[i], name)) != NULL)
			return s;
	return scopefind (globals, name);
}

/*
 * The locals of the function being compiled, in slot order: `l'
 * indices are assigned at install time, parameters first.
 */
struct symb	*ltab[NLOC];
int		 nlocidx, nargsloc;

struct symb *
install (name, sc)
int name;
int sc;
{
	struct symb *s;

	s = mksymb ();
	s->s_name = name;
	s->s_sc = sc;
	if (nscope > 0) {
		if (curfunc != NULL && sc != SC_STATIC
		    && sc != SC_TYPEDEF) {
			if (nlocidx >= NLOC)
				error ("too many locals");
			ltab[nlocidx++] = s;
		}
		s->s_next = scopes[nscope];
		scopes[nscope] = s;
	} else {
		s->s_next = globals;
		globals = s;
	}
	return s;
}

/* install into the global scope, wherever we are (implicit functions) */
struct symb *
ginstall (name, sc)
int name;
int sc;
{
	struct symb *s;

	s = mksymb ();
	s->s_name = name;
	s->s_sc = sc;
	s->s_next = globals;
	globals = s;
	return s;
}

void
blkpush ()
{
	if (++nscope >= NSCOPE)
		error ("blocks nested too deeply");
	scopes[nscope] = NULL;
}

/*
 * The symbols stay allocated until the function ends: their types
 * are written into the vars list at fdefend.  Shadowing chains are
 * simply unlinked.
 */
void
blkpop ()
{
	if (nscope <= 0)
		return;
	scopes[nscope] = NULL;
	--nscope;
}

void
freesymb (s)
struct symb *s;
{
	s->s_next = sfreelist;
	sfreelist = s;
}

/*
 * Install a function parameter (default int) in the current scope.
 * Returns NULL if the name is already taken.
 */
struct symb *
insparam (name)
int name;
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

static int	 ndspool;			/* type name pool high water */

/* reset the accumulated specifiers; the declarator and type name
   pools are recycled too, as their trees are consumed by the actions
   that precede every call of this */
void
dcl_reset ()
{
	curd.sc = 0;
	curd.bt = 0;
	curd.tp = NULL;
	nslots = 0;		/* declarators and exprs share slots */
	ndspool = 0;
}

void
sc_const ()
{
	curd.bt |= BT_CONST;
}

void
sc_sclass (sc)
int sc;
{
	if (curd.sc != 0)
		typerr ("two sclasses");
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
	int		 offs;		/* next member offset (structs) */
	int		 isunion;
} sustack[NSU];
static int nsu;

/*
 * A struct body is a nested declaration: the specifiers accumulated
 * for the enclosing declaration must survive the member declarations
 * inside (su_decl resets them after each member).
 */
static struct {
	int		 sc, bt;
	struct type	*tp;
} curdsave[NSU];

static struct type *
tagfind (name)
int name;
{
	struct symb *t;

	for (t = tags; t != NULL; t = t->s_next)
		if (t->s_name == name)
			return t->s_tp;
	return NULL;
}

static struct type *
tagtype (sou, name)
int sou;
int name;
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
int tag;
{
	struct type *tp;

	if (tag != 0) {
		tp = tagtype (sou, tag);
		if (tp->t_memb != 0)
			typerr ("redef %s", (int) namebuf (tag));
		tp->t_op = sou;
	} else
		tp = mktype (sou, NULL, 0, NULL, NULL);

	if (nsu >= NSU)
		error ("structs nested too deeply");
	curdsave[nsu].sc = curd.sc;
	curdsave[nsu].bt = curd.bt;
	curdsave[nsu].tp = curd.tp;
	dcl_reset ();
	sustack[nsu].su = tp;
	sustack[nsu].tail = &tp->t_memb;
	sustack[nsu].size = 0;
	sustack[nsu].offs = 0;
	sustack[nsu].isunion = (sou == T_UNION);
	++nsu;
	return tp;
}

struct type *
su_end ()
{
	struct type *tp;

	if (nsu == 0)
		error ("struct stack under");
	--nsu;
	tp = sustack[nsu].su;
	tp->t_size = sustack[nsu].size;
	curd.sc = curdsave[nsu].sc;
	curd.bt = curdsave[nsu].bt;
	curd.tp = curdsave[nsu].tp;
	return tp;
}

struct type *
su_ref (sou, tag)
int sou;
int tag;
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
	int name = dclname (d);
	int sz, align;

	if (nsu == 0) {
		typerr ("member outside su");
		return;
	}
	if (name == 0) {
		typerr ("no member name");
		return;
	}
	if (tp->t_op == T_FUNC) {
		typerr ("%s is a function", (int) namebuf (name));
		return;
	}
	if ((tp->t_op == T_STRUCT || tp->t_op == T_UNION)
	    && tp->t_memb == NULL) {
		typerr ("member %s incomplete", (int) namebuf (name));
		return;
	}
	if (scopefind (sustack[nsu - 1].su->t_memb, name) != NULL) {
		typerr ("dup member %s", (int) namebuf (name));
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
		m->s_sc = 0;
		if (sz > sustack[nsu - 1].size)
			sustack[nsu - 1].size = sz;
	} else {
		align = sz > 1 ? 2 : 1;
		sustack[nsu - 1].offs =
			(sustack[nsu - 1].offs + align - 1) & ~(align - 1);
		m->s_sc = sustack[nsu - 1].offs;
		sustack[nsu - 1].offs += sz;
		sustack[nsu - 1].size = sustack[nsu - 1].offs;
	}
}

/* ---------------- declarator parse trees ---------------- */


static struct dcl *
mkdcl (op)
int op;
{
	struct dcl *d;

	if (nslots >= NNNODE)
		error ("declarator complex");
	d = &slots[nslots++].d;
	d->d_op = op;
	d->d_l = NULL;
	d->d_u.d_size = NULL;
	d->d_u.d_name = 0;
	d->d_u.d_params = NULL;
	return d;
}

struct dcl *
dstar (op)
int op;
{
	if (op != '*')
		typerr ("want * in declarator");
	return mkdcl (D_PTR);
}

struct dcl *
dptrn (d, op)
struct dcl *d;
int op;
{
	struct dcl *p;

	if (op != '*')
		typerr ("want * in declarator");
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
int name;
{
	struct dcl *d = mkdcl (D_NAME);

	d->d_u.d_name = name;
	return d;
}

struct dcl *
dfunc (d, params)
struct dcl *d;
struct symb *params;
{
	struct dcl *f = mkdcl (D_FUNC);

	f->d_l = d;
	f->d_u.d_params = params;
	return f;
}

struct dcl *
dary (d, size)
struct dcl *d;
struct node *size;
{
	struct dcl *a = mkdcl (D_ARY);

	a->d_l = d;
	a->d_u.d_size = size;
	return a;
}

struct symb *
param1 (name)
int name;
{
	struct symb *p = mksymb ();

	p->s_name = name;
	return p;
}

struct symb *
paramn (list, name)
struct symb *list;
int name;
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
			t = ptrtype (t);
			break;
		case D_ARY:
			n = 0;
			if (d->d_u.d_size != NULL) {
				v = fold (d->d_u.d_size, &ok);
				if (!ok) {
					typerr ("size not const");
					n = 1;
				} else if (v < 0 || v > 65535) {
					typerr ("invalid array size");
					n = 1;
				} else
					n = (int) v;
			}
			t = mktype (T_ARY, t, n, NULL, NULL);
			break;
		case D_FUNC:
			t = mktype (T_FUNC, t, 0, d->d_u.d_params, NULL);
			break;
		default:
			error ("internal: bad declarator");
		}
		d = d->d_l;
	}
	return t;
}

int
dclname (d)
struct dcl *d;
{
	while (d != NULL && d->d_op != D_NAME)
		d = d->d_l;
	return d == NULL ? 0 : d->d_u.d_name;
}

/* ---------------- type names (casts, sizeof) ---------------- */

static struct dspec	dspool[NDSPEC];

static struct dspec *
mkdspec ()
{
	if (ndspool >= NDSPEC)
		error ("too many type names");
	return &dspool[ndspool++];
}

struct dspec *
tn_bt (bt)
int bt;
{
	struct dspec *p = mkdspec ();

	p->p_bt = bt;
	p->p_tp = NULL;
	return p;
}

struct dspec *
tn_td (sp)
struct symb *sp;
{
	struct dspec *p = mkdspec ();

	p->p_bt = 0;
	p->p_tp = sp->s_tp;
	return p;
}

struct dspec *
tn_su (tp)
struct type *tp;
{
	struct dspec *p = mkdspec ();

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
	int name = dclname (d);
	struct symb *p;

	/* K&R: array and function parameters arrive as pointers */
	if (tp->t_op == T_ARY)
		tp = ptrtype (tp->t_tp);
	else if (tp->t_op == T_FUNC)
		tp = ptrtype (tp);

	if (name == 0) {
		typerr ("no param name");
		return;
	}
	p = lookup (name);
	if (p == NULL || p->s_sc != SC_PARAM) {
		typerr ("%s not a param", (int) namebuf (name));
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
	int name = dclname (d);
	int sc;

	if (curd.sc == SC_TYPEDEF) {
		if (name == 0) {
			typerr ("no typedef name");
			return;
		}
		if (lookup (name) != NULL) {
			typerr ("redecl %s", (int) namebuf (name));
			return;
		}
		sp = install (name, SC_TYPEDEF);
		sp->s_tp = tp;
		return;
	}

	if (name == 0) {
		typerr ("no declarator name");
		return;
	}
	if (tp->t_op == T_FUNC) {
		/* declared function: redeclarations are fine */
		old = scopefind (globals, name);
		if (old != NULL && nscope == 0) {
			if (old->s_tp->t_op != T_FUNC)
				typerr ("redecl %s", (int) namebuf (name));
			else if (!compat (old->s_tp->t_tp, tp->t_tp))
				typerr ("return clash %s",
					(int) namebuf (name));
			old->s_tp = tp;
			chkinit (tp, init);
			return;
		}
		if (old != NULL && nscope > 0) {
			typerr ("redecl %s", (int) namebuf (name));
			return;
		}
		sc = curd.sc == 0 ? SC_EXTERN : curd.sc;
		sp = install (name, sc);
		sp->s_tp = tp;
		emit_fdecl (name, sc);
		return;
	} else {
		old = nscope > 0 ? scopefind (scopes[nscope], name)
				 : scopefind (globals, name);
		if (old != NULL) {
			if (nscope > 0 || !compat (old->s_tp, tp)
			    || old->s_sc == SC_TYPEDEF)
				typerr ("redecl %s", (int) namebuf (name));
			else
				old->s_tp = tp;
			chkinit (tp, init);
			return;
		}
		sc = curd.sc == 0 ? (nscope > 0 ? SC_AUTO : SC_GLOBAL)
				  : curd.sc;
	}

	sp = install (name, sc);
	sp->s_tp = tp;
	if ((tp->t_op == T_STRUCT || tp->t_op == T_UNION)
	    && tp->t_memb == NULL)
		typerr ("%s incomplete", (int) namebuf (name));
	else if (tp->t_op == (BT_VOID | BT_CONST)
		 || (tp->t_op & BT_MASK) == BT_VOID)
		typerr ("%s is void", (int) namebuf (name));
	chkinit (tp, init);
	if (nscope == 0) {
		emit_data (name, sc, tp, init);
		expr_reset ();		/* the init nodes are consumed */
	} else if (sc == SC_STATIC) {
		emit_static (sp, tp, init);
	} else if (init != NULL && init->n_op != O_ILIST) {
		/* an initialized auto becomes an assignment in the code */
		emstmt (nasgn ('=', nlocal (sp), init));
		expr_reset ();
	}
}

/* count the elements of an initializer list */
static int
countitems (e)
struct node *e;
{
	if (e == NULL)
		return 0;
	if (e->n_op != O_COMMA)
		return 1;
	return countitems (e->n_l) + countitems (e->n_r);
}

void
chkinit (tp, init)
struct type *tp;
struct node *init;
{
	struct node *list;
	struct symb *m;
	int n, count;

	if (init == NULL)
		return;
	if (init->n_op == O_ILIST) {
		list = init->n_l;
		if (tp->t_op == T_ARY) {
			if (list != NULL && list->n_op == O_STR
			    && ischar (tp->t_tp)) {
				if (tp->t_size == 0)
					tp->t_size = xstrnlen (list->n_val) + 1;
				return;
			}
			count = countitems (list);
			if (tp->t_size == 0)
				tp->t_size = count;
			if (tp->t_size != 0 && count > tp->t_size)
				typerr ("too many inits");
			chkitem (tp->t_tp, list);
		} else if (tp->t_op == T_STRUCT || tp->t_op == T_UNION) {
			n = 0;
			for (m = tp->t_memb; m != NULL; m = m->s_next)
				++n;
			count = countitems (list);
			if (count > n)
				typerr ("too many inits");
		}
		/* scalars take `{ expr }` as in K&R C */
		return;
	}

	if (tp->t_op == T_ARY) {
		if (init->n_op == O_STR
		    && ischar (tp->t_tp)) {
			if (tp->t_size == 0)
				tp->t_size = xstrnlen (init->n_val) + 1;
			return;
		}
		typerr ("array init needs {}");
		return;
	}
	if (!compat (tp, decay (init->n_tp)))
		typerr ("bad init type");
}

/* check each element of a braced initializer against tp */
static void
chkitem (tp, e)
struct type *tp;
struct node *e;
{
	if (e == NULL)
		return;
	if (e->n_op == O_COMMA) {
		chkitem (tp, e->n_l);
		chkitem (tp, e->n_r);
		return;
	}
	if (!compat (tp, decay (e->n_tp)))
		typerr ("bad init elem");
}

/* length of the string at offset off in the string file */
int
xstrnlen (off)
int off;
{
	char buf[32];
	int i, j, n, len;

	len = 0;
	for (i = 0; ; i += n) {
		n = pread (strfd, buf, sizeof buf, (long) (off + i));
		if (n <= 0)
			break;
		for (j = 0; j < n; ++j) {
			++len;
			if (buf[j] == '\0')
				return len - 1;
		}
	}
	return len;
}
