#include <string.h>
#include <unistd.h>
#include "c0.h"

/*
 * emit.c -- serializes the IR described in ir-fmt.  Every expression
 * node is followed by a type byte, so c1 knows the width and
 * signedness of each operand.  `g' and `l' denote addresses; `*'
 * loads, `=' stores.  Pointer arithmetic is scaled to bytes here.
 * String literals become anonymous static data (.sN), referenced and
 * relocated by symbol.
 */


static int	 symnames[NSYMNM];	/* string file offsets */
static int	 nsymnames;

/* static locals: symbol -> mangled name */
static struct symb *sttab[NSTATIC];
static int	  stname[NSTATIC];
static int	 nstatic;

/* ltab/nlocidx/nargsloc come from decl.c/stmt.c */

void		emexpr ();
static void	emaddr (), embin (), emit_init ();

/* ---------------- output helpers ---------------- */

static void
wbyte (v)
int v;
{
	oputc (v & 0xff, firfd);
}

static void
wword (v)
int v;
{
	oputc (v & 0xff, firfd);
	oputc ((v >> 8) & 0xff, firfd);
}

static void
wdword (v)
long v;
{
	int i;

	for (i = 0; i < 4; ++i) {
		oputc ((int) (v & 0xff), firfd);
		v >>= 8;
	}
}

/* append the decimal representation of n to s, return the end */
char *
numstr (s, n)
char *s;
int n;
{
	char buf[8];
	int i = 0;

	if (n == 0) {
		*s++ = '0';
		return s;
	}
	while (n > 0) {
		buf[i++] = '0' + n % 10;
		n /= 10;
	}
	while (i > 0)
		*s++ = buf[--i];
	return s;
}

/* "a.b" from two string file offsets, interned */
static int
genname (a, b)
int a, b;
{
	char bufa[48], bufb[48], buf[96], *p;
	int i, n;

	n = pread (strfd, bufa, sizeof bufa - 1, (long) a);
	if (n < 0)
		n = 0;
	bufa[n] = '\0';
	n = pread (strfd, bufb, sizeof bufb - 1, (long) b);
	if (n < 0)
		n = 0;
	bufb[n] = '\0';
	p = buf;
	for (i = 0; bufa[i] != '\0' && p < buf + sizeof buf - 1; ++i)
		*p++ = bufa[i];
	*p++ = '.';
	for (i = 0; bufb[i] != '\0' && p < buf + sizeof buf - 1; ++i)
		*p++ = bufb[i];
	*p = '\0';
	return intern (buf);
}

static int
emitsym (name)
int name;
{
	char buf[48];
	int i, n, t;

	for (i = 0; i < nsymnames; ++i)
		if (symnames[i] == name)
			return i;
	if (nsymnames >= NSYMNM)
		error ("too many symbols");
	symnames[nsymnames] = name;
	for (t = 0; ; t += n) {
		n = pread (strfd, buf, sizeof buf, (long) (name + t));
		if (n <= 0)
			break;
		for (i = 0; i < n; ++i) {
			oputc (buf[i], symfd);
			if (buf[i] == '\0')
				return nsymnames++;
		}
	}
	oputc (0, symfd);
	return nsymnames++;
}

static void
wsym (name)
int name;
{
	wword (emitsym (name));
}

/* the type byte of a type, plus a following word for blocks */
static void
wty (t, size)
struct type *t;
int size;
{
	int ty, w;

	if (t == NULL)
		w = 2;
	else
		w = tysize (t);
	ty = w == 1 ? 0 : w == 2 ? 1 : w == 4 ? 2 : 3;
	if (t != NULL && (t->t_op == T_PTR || t->t_op == T_ARY
			  || t->t_op == T_FUNC
			  || (t->t_op < 0x100 && (t->t_op & BT_UNSIGNED))))
		ty |= 0x04;
	wbyte (ty);
	if ((ty & 3) == 3)
		wword (size);
}

static void
wtyaddr ()
{
	wbyte (0x05);			/* word, unsigned */
}

static void
wconst (v, t)
long v;
struct type *t;
{
	if (t != NULL && tysize (t) > 2) {
		wbyte ('I');
		wty (t, 4);
		wdword (v);
	} else if (v < -32768 || v > 65535) {
		wbyte ('I');
		wty (btype (BT_INT | BT_LONG), 4);
		wdword (v);
	} else {
		wbyte ('i');
		wty (t == NULL ? btype (BT_INT) : t, 2);
		wword ((int) v);
	}
}

/* ---------------- string literals ---------------- */

/* ---------------- static locals ---------------- */

static int
staticname (sp)
struct symb *sp;
{
	int i;

	for (i = 0; i < nstatic; ++i)
		if (sttab[i] == sp)
			return stname[i];
	return 0;
}

void
emit_static (sp, tp, init)
struct symb *sp;
struct type *tp;
struct node *init;
{
	int name;

	name = genname (curfunc->s_name, sp->s_name);
	if (nstatic >= NSTATIC)
		error ("too many static locals");
	sttab[nstatic] = sp;
	stname[nstatic] = name;
	++nstatic;
	emit_data (name, SC_STATIC, tp, init);
}

/* ---------------- global definitions ---------------- */

static int	icnt;			/* bytes of init written so far */

static void
izbytes (n)
int n;
{
	wbyte (0x01);
	wword (n);
	icnt += n;
	while (n-- > 0)
		wbyte (0);
}

static void
iscalar (sz, v)
int sz;
long v;
{
	wbyte (0x01);
	wword (sz);
	icnt += sz;
	while (sz-- > 0) {
		wbyte ((int) (v & 0xff));
		v >>= 8;
	}
}

static void
ireloc (name, off)
int name;
long off;
{
	wbyte (0x02);
	wsym (name);
	wword ((int) off);
	icnt += 2;
}

/* a word holding the address of a string file entry */
static void
istrreloc (six, off)
int six;
long off;
{
	wbyte (0x03);
	wword (six);
	wword ((int) off);
	icnt += 2;
}

static void
ipad (to)
int to;
{
	while (icnt < to)
		izbytes (1);
}

/*
 * Try to evaluate e as a constant address (symbol + offset).
 */
/*
 * Constant address of e: either a symbol name (string file offset)
 * or a string file entry; *pstr says which.
 */
static int
constaddr (e, pname, poff, pstr)
struct node *e;
int *pname;
long *poff;
int *pstr;
{
	struct symb *sp, *m;
	struct type *t;
	long v;
	int ok, sz;

	*pstr = 0;
	switch (e->n_op) {
	case O_STR:
		*pname = e->n_val;
		*poff = 0;
		*pstr = 1;
		return 1;
	case O_ADDR:
		return constaddr (e->n_l, pname, poff, pstr);
	case O_NAME:
		sp = (struct symb *) e->n_l;
		if (sp->s_sc == SC_AUTO || sp->s_sc == SC_PARAM
		    || sp->s_sc == SC_REGISTER)
			return 0;	/* address of a local */
		if (staticname (sp) != 0)
			*pname = staticname (sp);
		else
			*pname = sp->s_name;
		*poff = 0;
		return 1;
	case O_INDEX:
		t = decay (e->n_l->n_tp);
		sz = isptr (t) ? tysize (t->t_tp) : 1;
		if (isarith (decay (e->n_r->n_tp))) {
			/* constant indexing is enough for initializers */
			v = fold (e->n_r, &ok);
			if (ok && constaddr (e->n_l, pname, poff, pstr)) {
				*poff += v * sz;
				return 1;
			}
		} else {
			v = fold (e->n_l, &ok);
			if (ok && constaddr (e->n_r, pname, poff, pstr)) {
				*poff += v * sz;
				return 1;
			}
		}
		return 0;
	case O_MEMBER:
	case O_ARROW:
		m = (struct symb *) e->n_r;
		if (e->n_op == O_ARROW)
			return 0;
		if (constaddr (e->n_l, pname, poff, pstr)) {
			*poff += m->s_sc;
			return 1;
		}
		return 0;
	}
	return 0;
}

/* flatten an initializer list into iitems[], in order */
static struct node	*iitems[NITEM];
static int		 niitems;

static void
flatten (e)
struct node *e;
{
	if (e == NULL)
		return;
	if (e->n_op == O_COMMA) {
		flatten (e->n_l);
		flatten (e->n_r);
		return;
	}
	if (niitems < 128)
		iitems[niitems++] = e;
}

static void
emit_init (tp, e)
struct type *tp;
struct node *e;
{
	struct symb *m;
	int name, isstr;
	long v, off;
	int ok, sz, i;

	if (e == NULL)
		return;
	switch (tp->t_op) {
	case T_ARY:
		if (e->n_op == O_STR && ischar (tp->t_tp)) {
			char buf[32];
			int j, n;

			sz = strnlen (e->n_val);
			wbyte (0x01);
			wword (sz + 1);
			for (i = 0; ; i += n) {
				n = pread (strfd, buf, sizeof buf,
					   (long) (e->n_val + i));
				if (n <= 0)
					break;
				for (j = 0; j < n; ++j) {
					wbyte (buf[j]);
					if (buf[j] == '\0')
						goto strdone;
				}
			}
		strdone:
			icnt += sz + 1;
			return;
		}
		niitems = 0;
		flatten (e->n_op == O_ILIST ? e->n_l : e);
		for (i = 0; i < niitems; ++i)
			emit_init (tp->t_tp, iitems[i]);
		return;
	case T_STRUCT:
	case T_UNION:
		niitems = 0;
		flatten (e->n_op == O_ILIST ? e->n_l : e);
		i = 0;
		for (m = tp->t_memb; m != NULL && i < niitems;
		     m = m->s_next) {
			ipad (m->s_sc);
			emit_init (m->s_tp, iitems[i++]);
			if (tp->t_op == T_UNION)
				break;
		}
		return;
	default:
		if (constaddr (e, &name, &off, &isstr)) {
			if (isstr)
				istrreloc (name, off);
			else
				ireloc (name, off);
			return;
		}
		v = fold (e, &ok);
		if (!ok) {
			typerr ("init not const");
			izbytes (tysize (tp));
			return;
		}
		iscalar (tysize (tp), v);
		return;
	}
}

void
emit_data (name, sc, tp, init)
int name;
int sc;
struct type *tp;
struct node *init;
{
	int flags = sc == SC_STATIC ? 2 : sc == SC_EXTERN ? 0 : 1;

	if (init != NULL)
		flags |= 4;
	wbyte ('D');
	wbyte (flags);
	wsym (name);
	wword (tysize (tp));
	if (init != NULL) {
		icnt = 0;
		emit_init (tp, init);
		wbyte (0x00);
	}
}

void
emit_fdecl (name, sc)
int name;
int sc;
{
	wbyte ('F');
	wbyte (sc == SC_STATIC ? 2 : 0);
	wsym (name);
	wbyte (0xff);
}

/* ---------------- expressions ---------------- */

/* byte codes for the multi-character binary operators */
static const char opcodes[] = {
	'E', 'N', 'L', 'G', 'l', 'r', 'A', 'O'
};

static int
opbyte (op)
int op;
{
	if (op >= O_EQ && op <= O_OROR)
		return opcodes[op - O_EQ];
	return op;		/* '+', '-', '*', '/', '%', '&', '|', '^',
				   '<', '>' */
}

/* the plain operator of a compound assignment */
static const char asgops[] = {
	'+', '-', '*', '/', '%', '&', '|', '^', 'l', 'r'
};

static int
baseop (op)
int op;
{
	if (op >= O_ADDA && op <= O_RSA)
		return asgops[op - O_ADDA];
	return O_RS;
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

static void
emargs (e)
struct node *e;
{
	if (e == NULL)
		return;
	if (e->n_op == O_COMMA) {
		emargs (e->n_l);
		emexpr (e->n_r, NULL);
	} else
		emexpr (e, NULL);
}

static void
emsymref (sp)
struct symb *sp;
{
	int i;

	for (i = 0; i < nlocidx; ++i)
		if (ltab[i] == sp) {
			wbyte ('l');
			wtyaddr ();
			wbyte (i);
			return;
		}
	wbyte ('g');
	wtyaddr ();
	wsym (staticname (sp) != 0 ? staticname (sp) : sp->s_name);
}

/* emit e scaled by sz (pointer indexing) */
static void
emscaled (e, sz)
struct node *e;
int sz;
{
	if (sz <= 1) {
		emexpr (e, NULL);
		return;
	}
	if (e->n_op == O_CON) {
		wconst (e->n_val * sz, e->n_tp);
		return;
	}
	wbyte ('b');
	wty (btype (BT_INT), 2);
	wbyte ('*');
	emexpr (e, NULL);
	wconst ((long) sz, btype (BT_INT));
}

static void
embin (op, ty, l, r)
int op;
struct type *ty;
struct node *l, *r;
{
	struct type *lt, *rt;
	int sz;

	lt = decay (l->n_tp);
	rt = decay (r->n_tp);

	if (op == '-' && isptr (lt) && isptr (rt)) {
		/* pointer difference: (l - r) / element size */
		sz = tysize (lt->t_tp);
		wbyte ('b');
		wty (ty, 2);
		wbyte ('/');
		wbyte ('b');
		wty (lt, 2);
		wbyte ('-');
		emexpr (l, NULL);
		emexpr (r, NULL);
		wconst ((long) sz, btype (BT_INT));
		return;
	}

	wbyte ('b');
	wty (ty, tysize (ty));
	wbyte (opbyte (op));
	if (op == '+' && isptr (lt) && isarith (rt)) {
		emexpr (l, NULL);
		emscaled (r, tysize (lt->t_tp));
	} else if (op == '+' && isarith (lt) && isptr (rt)) {
		emscaled (l, tysize (rt->t_tp));
		emexpr (r, NULL);
	} else if (op == '-' && isptr (lt) && isarith (rt)) {
		emexpr (l, NULL);
		emscaled (r, tysize (lt->t_tp));
	} else {
		emexpr (l, NULL);
		emexpr (r, NULL);
	}
}

/* the address of an lvalue */
static void
emaddr (e)
struct node *e;
{
	struct symb *m;
	struct type *lt, *rt;
	int sz;

	switch (e->n_op) {
	case O_NAME:
		emsymref ((struct symb *) e->n_l);
		return;
	case '*':
		emexpr (e->n_l, NULL);
		return;
	case O_INDEX:
		lt = decay (e->n_l->n_tp);
		rt = decay (e->n_r->n_tp);
		if (isptr (lt)) {
			sz = tysize (lt->t_tp);
			wbyte ('b');
			wtyaddr ();
			wbyte ('+');
			emexpr (e->n_l, NULL);
			emscaled (e->n_r, sz);
		} else {
			sz = tysize (rt->t_tp);
			wbyte ('b');
			wtyaddr ();
			wbyte ('+');
			emexpr (e->n_r, NULL);
			emscaled (e->n_l, sz);
		}
		return;
	case O_MEMBER:
	case O_ARROW:
		m = (struct symb *) e->n_r;
		wbyte ('b');
		wtyaddr ();
		wbyte ('+');
		if (e->n_op == O_MEMBER)
			emaddr (e->n_l);
		else
			emexpr (e->n_l, NULL);
		wconst ((long) m->s_sc, btype (BT_INT));
		return;
	default:
		/* not an lvalue; the type checker complained already */
		emexpr (e, NULL);
		return;
	}
}

/*
 * Emit an expression.  oty overrides the type written for this node
 * (casts); nested operands keep their own types, and c1 reconciles
 * the widths.
 */
void
emexpr (e, oty)
struct node *e;
struct type *oty;
{
	struct type *ty;
	struct symb *sp;
	int sz, ispre;

	if (e == NULL) {
		wconst (0, btype (BT_INT));
		return;
	}
	ty = oty != NULL ? oty : e->n_tp;

	switch (e->n_op) {
	case O_CON:
	case O_LCON:
		wconst (e->n_op == O_LCON ? lcons[e->n_val]
					   : (long) e->n_val, ty);
		return;
	case O_STR:
		wbyte ('S');
		wword (e->n_val);
		return;
	case O_NAME:
		sp = (struct symb *) e->n_l;
		if (sp->s_tp->t_op == T_ARY || sp->s_tp->t_op == T_FUNC) {
			emsymref (sp);		/* decays to its address */
			return;
		}
		wbyte ('*');
		wty (ty, tysize (sp->s_tp));
		emsymref (sp);
		return;
	case O_CAST:
		emexpr (e->n_l, e->n_tp);
		return;
	case O_ADDR:
		emaddr (e->n_l);
		return;
	case '*':
		wbyte ('*');
		wty (ty, tysize (e->n_tp));
		emexpr (e->n_l, NULL);
		return;
	case O_INDEX:
	case O_MEMBER:
	case O_ARROW:
		wbyte ('*');
		wty (ty, tysize (e->n_tp));
		emaddr (e);		/* the plain address, then deref */
		return;
	case '!':
	case '~':
		wbyte ('u');
		wty (ty, tysize (e->n_tp));
		wbyte (e->n_op);
		emexpr (e->n_l, NULL);
		return;
	case '+':
	case '-':
		if (e->n_r == NULL) {
			if (e->n_op == '+') {
				emexpr (e->n_l, oty);
				return;
			}
			wbyte ('u');
			wty (ty, tysize (e->n_tp));
			wbyte ('-');
			emexpr (e->n_l, NULL);
			return;
		}
		embin (e->n_op, ty, e->n_l, e->n_r);
		return;
	case O_PREINC:
	case O_PREDEC:
	case O_POSTINC:
	case O_POSTDEC:
		ispre = e->n_op == O_PREINC || e->n_op == O_PREDEC;
		sz = isptr (decay (e->n_tp)) ? tysize (e->n_tp->t_tp) : 1;
		wbyte ('n');
		wty (e->n_tp, tysize (e->n_tp));
		wbyte (e->n_op == O_PREINC || e->n_op == O_POSTINC
			 ? '+' : '-');
		wbyte (ispre);
		wword (sz);
		emaddr (e->n_l);
		return;
	case '=':
		wbyte ('=');
		wty (ty, tysize (e->n_tp));
		emaddr (e->n_l);
		emexpr (e->n_r, NULL);
		return;
	case O_ADDA:
	case O_SUBA:
	case O_MULA:
	case O_DIVA:
	case O_MODA:
	case O_ANDA:
	case O_ORA:
	case O_XORA:
	case O_LSA:
	case O_RSA:
		/* dest = dest op src, with the value of dest reloaded */
		wbyte ('=');
		wty (ty, tysize (e->n_tp));
		emaddr (e->n_l);
		embin (baseop (e->n_op), e->n_tp, e->n_l, e->n_r);
		return;
	case O_COMMA:
		wbyte (',');
		wty (ty, tysize (e->n_tp));
		emexpr (e->n_l, NULL);
		emexpr (e->n_r, NULL);
		return;
	case O_COND:
		wbyte ('?');
		wty (ty, tysize (e->n_tp));
		emexpr (e->n_l, NULL);
		emexpr (e->n_r, NULL);
		emexpr ((struct node *) (int) e->n_val, NULL);
		return;
	case O_CALL:
		wbyte ('c');
		wty (ty, tysize (e->n_tp));
		wbyte (argcount (e->n_r));
		emexpr (e->n_l, NULL);
		emargs (e->n_r);
		return;
	default:
		embin (e->n_op, ty, e->n_l, e->n_r);
		return;
	}
}

/* ---------------- functions ---------------- */

void
emit_fhead (name, sc)
int name;
int sc;
{
	wbyte ('F');
	wbyte ((sc == SC_STATIC ? 2 : 1) | 4);
	wsym (name);
}

void
emit_ftail (tp)
struct type *tp;
{
	int i;

	(void) tp;
	wbyte (0xff);			/* end of code */
	for (i = 0; i < nargsloc; ++i)
		wword (tysize (ltab[i]->s_tp));
	wbyte (0xff);
	wbyte (0xff);
	for (; i < nlocidx; ++i)
		wword (tysize (ltab[i]->s_tp));
	wbyte (0xff);
	wbyte (0xff);
}

/*
 * A generated label is written as a sym word with the high bit set:
 * c1 reads the low 15 bits as the label number.  No name, no symbol
 * file entry.
 */
void
wlab (lab)
int lab;
{
	wword (0x8000 | lab);
}

/* label, jump, branch, return, switch chain: the statement forms */
void
emlab (lab)
int lab;
{
	wbyte ('L');
	wlab (lab);
}

void
emjump (lab)
int lab;
{
	wbyte ('J');
	wlab (lab);
}

/* a user label: written as a plain symbol name */
void
emusym (lab, kind)
int lab;
int kind;			/* 'L' or 'J' */
{
	wbyte (kind);
	wsym (lab);
}

void
embr (lt, lf, cond)
int lt, lf;
struct node *cond;
{
	wbyte ('B');
	wlab (lt);
	wlab (lf);
	emexpr (cond, NULL);
}

void
emret (rt, e)
struct type *rt;
struct node *e;
{
	if (e == NULL) {
		wbyte ('r');
		return;
	}
	wbyte ('R');
	wty (rt, tysize (rt));
	emexpr (e, NULL);
}

void
emswch (t, cases, dflt, endlab)
struct node *t;
struct swcase *cases;
int dflt, endlab;
{
	struct swcase *c;

	for (c = cases; c != NULL; c = c->next) {
		wbyte ('B');
		wlab (c->lab);
		if (c->next != NULL)
			wlab (c->next->lab);
		else if (dflt != 0)
			wlab (dflt);
		else
			wlab (endlab);
		wbyte ('b');
		wty (btype (BT_INT), 2);
		wbyte ('E');
		emexpr (t, NULL);
		wconst ((long) c->val, btype (BT_INT));
	}
	if (dflt != 0)
		emjump (dflt);
}
