#include <_varargs.h>
#include <string.h>
#include <stdlib.h>
#include <ctype.h>
#include "c0.h"

/*
 * lex.c -- lexical analyser for c0, the first pass of the C compiler.
 *
 * Operators that share a precedence level share a token class (ASGN,
 * EQOP, RELOP, ...); the exact operator is passed in yylval.  An
 * identifier that names a typedef is returned as TYPENAME, with its
 * symbol in yylval, so the parser can tell casts and declarations
 * apart from expressions.
 */

int		linenum = 1;
long		numval;
int		numbt;

static int	bol = 1;
static int	sidx;			/* string bytes written so far */

/*
 * The string bytes are collected here, so that the IR emitter can
 * define string literals as data records; the index stored in a
 * STRING token is an offset into this array.
 */
char		sdata[NSDATA];

struct kwent {
	char	*k_name;
	int	 k_tok;
	int	 k_val;
};

static const struct kwent kwtab[] = {
	{ "sizeof",	SIZEOF,		0 },
	{ "goto",	GOTO,		0 },
	{ "if",		IF,		0 },
	{ "else",	ELSE,		0 },
	{ "while",	WHILE,		0 },
	{ "do",		DO,		0 },
	{ "for",	FOR,		0 },
	{ "return",	RETURN,		0 },
	{ "break",	BREAK,		0 },
	{ "continue",	CONTINUE,	0 },
	{ "switch",	SWITCH,		0 },
	{ "case",	CASE,		0 },
	{ "default",	DEFAULT,	0 },
	{ "const",	CONST,		0 },
	{ "auto",	SCLASS,		SC_AUTO },
	{ "static",	SCLASS,		SC_STATIC },
	{ "extern",	SCLASS,		SC_EXTERN },
	{ "register",	SCLASS,		SC_REGISTER },
	{ "typedef",	SCLASS,		SC_TYPEDEF },
	{ "void",	TYPEKW,		BT_VOID },
	{ "char",	TYPEKW,		BT_CHAR },
	{ "short",	TYPEKW,		BT_SHORT },
	{ "int",	TYPEKW,		BT_INT },
	{ "long",	TYPEKW,		BT_LONG },
	{ "signed",	TYPEKW,		BT_SIGNED },
	{ "unsigned",	TYPEKW,		BT_UNSIGNED },
	{ "struct",	SOU,		T_STRUCT },
	{ "union",	SOU,		T_UNION },
	{ NULL,		0,		0 },
};

/* interned identifier strings, kept for the life of the run */

static char	 narena[NNAME];
static int	 narenai;
static char	*itab[NINT];
static int	 nint;

static char *
intern (s)
char *s;
{
	char	*copy;
	int	 i;

	for (i = 0; i < nint; ++i)
		if (strcmp (itab[i], s) == 0)
			return itab[i];

	copy = &narena[narenai];
	while (*s != '\0') {
		if (narenai >= NNAME)
			error ("out of space for identifiers");
		narena[narenai++] = *s++;
	}
	narena[narenai++] = '\0';
	if (nint < NINT)
		itab[nint++] = copy;
	return copy;
}

__dead void
error (fmt)
const char *fmt;
{
	va_list ap;
	int a1, a2;

	va_start (ap, fmt);
	a1 = va_arg (ap, int);
	a2 = va_arg (ap, int);
	va_end (ap);
	mini (2, "%d: ", linenum, 0);
	mini (2, fmt, a1, a2);
	oputc ('\n', 2);
	exit (1);
}

static int
get ()
{
	int ch;

	ch = c0getc ();
	if (ch == '\n') {
		++linenum;
		bol = 1;
	} else if (ch != ' ' && ch != '\t' && ch != '#')
		bol = 0;
	return ch;
}

static void
unget (ch)
int ch;
{
	if (ch == '\n') {
		--linenum;
		bol = 0;
	}
	c0ungetc (ch);
}

static int
digval (ch, base)
int ch, base;
{
	int d;

	if (isdigit (ch))
		d = ch - '0';
	else if (base == 16 && isxdigit (ch))
		d = isupper (ch) ? ch - 'A' + 10 : ch - 'a' + 10;
	else
		return -1;
	return d < base ? d : -1;
}

static int
escape (ch)
int ch;
{
	int n, nd;

	switch (ch) {
	case 'n':
		return '\n';
	case 't':
		return '\t';
	case 'r':
		return '\r';
	case 'b':
		return '\b';
	case 'f':
		return '\f';
	case 'v':
		return '\v';
	case '0':
	case '1':
	case '2':
	case '3':
	case '4':
	case '5':
	case '6':
	case '7':
		n = ch - '0';
		nd = 1;
		while (nd < 3) {
			ch = get ();
			if (ch < '0' || ch > '7') {
				unget (ch);
				break;
			}
			n = n * 8 + ch - '0';
			++nd;
		}
		return n & 0xff;
	default:
		return ch;		/* \' \" \\ \? */
	}
}

static int
lex ()
{
	char	 id[MAXIDENT + 1];
	char	*name;
	struct symb *sp;
	const struct kwent *kw;
	long	 v;
	int	 ch, c2, d, base, i, unsf, longf;

	/* skip whitespace, comments and preprocessor control lines */
	for (;;) {
		ch = get ();
		if (ch == ' ' || ch == '\t' || ch == '\n' || ch == '\r'
		    || ch == '\f' || ch == '\v')
			continue;
		if (ch == '#' && bol) {
			/* line marker `# 12 "file"`: set the line number
			   so errors name the real line */
			c2 = get ();
			for (d = 0; isdigit (c2); c2 = get ())
				d = d * 10 + c2 - '0';
			if (d != 0)
				linenum = d - 1;
			while (c2 != '\n' && c2 != EOF)
				c2 = get ();
			continue;
		}
		if (ch == '/') {
			c2 = get ();
			if (c2 == '/') {
				do
					ch = get ();
				while (ch != '\n' && ch != EOF);
				continue;
			}
			if (c2 == '*') {
				ch = get ();
				for (;;) {
					if (ch == EOF)
						error ("unterminated comment");
					if (ch == '*') {
						ch = get ();
						if (ch == '/')
							break;
						continue;
					}
					ch = get ();
				}
				continue;
			}
			unget (c2);
		}
		break;
	}

	if (ch == EOF)
		return 0;

	if ((d = digval (ch, 10)) >= 0) {
		base = 10;
		if (ch == '0') {
			c2 = get ();
			if (c2 == 'x' || c2 == 'X') {
				base = 16;
				ch = get ();
			} else {
				base = 8;
				unget (c2);
			}
		}
		v = 0;
		while ((d = digval (ch, base)) >= 0) {
			v = v * base + d;
			ch = get ();
		}
		if (base == 8 && digval (ch, 10) >= 0)
			error ("invalid digit in octal constant");
		unsf = longf = 0;
		while (ch == 'u' || ch == 'U' || ch == 'l' || ch == 'L') {
			if (ch == 'u' || ch == 'U')
				unsf = 1;
			else
				longf = 1;
			ch = get ();
		}
		unget (ch);
		if (!longf) {
			if (v > 0x7fff)
				unsf = 1;
			if (v > 0xffff)
				longf = 1;
		}
		numval = v;
		numbt = BT_INT;
		if (unsf)
			numbt |= BT_UNSIGNED;
		if (longf)
			numbt |= BT_LONG;
		return INTEGER;
	}

	if (isalpha (ch) || ch == '_') {
		i = 0;
		do {
			if (i < MAXIDENT)
				id[i++] = ch;
			ch = get ();
		} while (isalnum (ch) || ch == '_');
		id[i] = '\0';
		unget (ch);

		for (kw = kwtab; kw->k_name != NULL; ++kw) {
			if (strcmp (kw->k_name, id) == 0) {
				yylval = kw->k_val;
				return kw->k_tok;
			}
		}

		name = intern (id);
		sp = lookup (name);
		if (sp != NULL && sp->s_sc == SC_TYPEDEF) {
			yylval = (int) sp;
			return TYPENAME;
		}
		yylval = (int) name;
		return IDENTIFIER;
	}

	switch (ch) {
	case '(':
	case ')':
	case '[':
	case ']':
	case '{':
	case '}':
	case ',':
	case ':':
	case ';':
	case '?':
		return ch;
	case '\'':
		v = 0;
		for (;;) {
			ch = get ();
			if (ch == '\'' || ch == EOF)
				break;
			if (ch == '\\')
				ch = escape (get ());
			v = (v << 8) | (ch & 0xff);
		}
		numval = v;
		numbt = BT_INT;
		return INTEGER;
	case '"':
		v = sidx;
		while ((ch = get ()) != '"') {
			if (ch == EOF)
				error ("unterminated string constant");
			if (ch != '\\')
				goto put;
			ch = escape (get ());
		put:
			if (sidx >= (int) sizeof (sdata))
				error ("too much string data");
			sdata[sidx] = ch;
			++sidx;
		}
		if (sidx >= (int) sizeof (sdata))
			error ("too much string data");
		sdata[sidx] = '\0';
		++sidx;
		yylval = (int) v;
		return STRING;
	case '.':
		yylval = O_MEMBER;
		return MBROP;
	case '~':
		yylval = '~';
		return UNOP;
	case '&':
		c2 = get ();
		if (c2 == '&')
			return ANDAND;
		if (c2 == '=') {
			yylval = O_ANDA;
			return ASGN;
		}
		unget (c2);
		yylval = '&';
		return '&';
	case '|':
		c2 = get ();
		if (c2 == '|')
			return OROR;
		if (c2 == '=') {
			yylval = O_ORA;
			return ASGN;
		}
		unget (c2);
		yylval = '|';
		return '|';
	case '^':
		c2 = get ();
		if (c2 == '=') {
			yylval = O_XORA;
			return ASGN;
		}
		unget (c2);
		yylval = '^';
		return '^';
	case '=':
		c2 = get ();
		if (c2 == '=') {
			yylval = O_EQ;
			return EQOP;
		}
		unget (c2);
		yylval = '=';
		return ASGN;
	case '!':
		c2 = get ();
		if (c2 == '=') {
			yylval = O_NE;
			return EQOP;
		}
		unget (c2);
		yylval = '!';
		return UNOP;
	case '+':
	case '-':
		c2 = get ();
		if (c2 == ch) {
			yylval = ch;
			return INCOP;
		}
		if (ch == '-' && c2 == '>') {
			yylval = O_ARROW;
			return MBROP;
		}
		if (c2 == '=') {
			yylval = ch == '+' ? O_ADDA : O_SUBA;
			return ASGN;
		}
		unget (c2);
		yylval = ch;
		return ADDOP;
	case '*':
	case '/':
	case '%':
		c2 = get ();
		if (c2 == '=') {
			yylval = ch == '*' ? O_MULA
			      : ch == '/' ? O_DIVA : O_MODA;
			return ASGN;
		}
		unget (c2);
		yylval = ch;
		return MULOP;
	case '<':
	case '>':
		/* < <= << <<=  and  > >= >> >>= */
		c2 = get ();
		if (c2 == ch) {
			d = get ();
			if (d == '=') {
				yylval = ch == '<' ? O_LSA : O_RSA;
				return ASGN;
			}
			unget (d);
			yylval = ch == '<' ? O_LS : O_RS;
			return SHIFT;
		}
		if (c2 == '=') {
			yylval = ch == '<' ? O_LE : O_GE;
			return RELOP;
		}
		unget (c2);
		yylval = ch;
		return RELOP;
	default:
		error ("invalid character '%c' (0%o)", ch, ch);
	}
	/* NOTREACHED */
}

/*
 * One token of pushback, so that the parser actions can peek at the
 * next token without disturbing the parser's own lookahead.
 */
static int	peektok = -9;		/* -9: no token pushed back */
static int	peeklval;
static long	peeknumval;
static int	peeknumbt;

/* concatenate two string literals; returns the new index */
int
strconcat (ai, bi)
int ai, bi;
{
	int ni = sidx;
	int i;

	for (i = ai; sdata[i] != '\0'; ++i) {
		if (sidx >= (int) sizeof (sdata))
			error ("too much string data");
		sdata[sidx++] = sdata[i];
	}
	for (i = bi; sdata[i] != '\0'; ++i) {
		if (sidx >= (int) sizeof (sdata))
			error ("too much string data");
		sdata[sidx++] = sdata[i];
	}
	if (sidx >= (int) sizeof (sdata))
		error ("too much string data");
	sdata[sidx++] = '\0';
	return ni;
}

int
lexpeek ()
{
	int	 sl;
	long	 sn;
	int	 sb;

	if (peektok != -9)
		return peektok;
	sl = yylval;
	sn = numval;
	sb = numbt;
	peektok = lex ();
	peeklval = yylval;
	peeknumval = numval;
	peeknumbt = numbt;
	yylval = sl;
	numval = sn;
	numbt = sb;
	return peektok;
}

int
yylex ()
{
	int t;

	if (peektok == -9)
		return lex ();
	t = peektok;
	peektok = -9;
	yylval = peeklval;
	numval = peeknumval;
	numbt = peeknumbt;
	return t;
}
