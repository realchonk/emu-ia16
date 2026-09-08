#ifndef FILE_C0_H
#define FILE_C0_H
#include <stddef.h>
#include <cdefs.h>

#define EOF	(-1)

/* output file descriptors; input is file descriptor 0 */
extern int	firfd, symfd;

#define MAXIDENT	31

/*
 * Pool and table sizes.  Everything c0 allocates comes from static
 * pools sized here; code, parser tables and pools together must stay
 * below the 64K segment, with room left for the stack.
 */

/* lex.c */
#define	NNAME	6144			/* identifier arena, bytes */
#define	NINT	640			/* interned identifiers */
#define	NSDATA	1280			/* string literal bytes */

/* type.c */
#define	NTYPE	208			/* type pool entries */

/* decl.c */
#define	NSYMB	384			/* symbol pool entries */
#define	NSCOPE	16			/* block nesting depth */
#define	NSU	16			/* struct/union nesting depth */
#define	NDCL	184			/* declarator tree nodes */
#define	NDSPEC	80			/* type name specifiers */

/* expr.c */
#define	NNODE	720			/* expression tree nodes */
#define	NLCON	16			/* long constants (O_LCON) */

/* stmt.c */
#define	NSREC	256			/* lowered statements per function */
#define	NCASE	60			/* case labels per file */
#define	NLAB	64			/* labels per function */
#define	NNEST	32			/* nested loops, ifs, switches */
#define	NSW	8			/* nested switches */
#define	NLOC	128			/* params and locals per function */
#define	NLABCH	320			/* label name arena, bytes */

/* emit.c */
#define	NSYMNM	256			/* names in the symbol file */
#define	NNAMEAR	1024			/* generated names, bytes */
#define	NSTRDEF	96			/* distinct string literals */
#define	NSTATIC	48			/* static locals */
#define	NITEM	128			/* flattened initializer items */

/* parse.o (y.tab.c) */
#define	YYMAXDEPTH	64		/* parser stack depth */

/*
 * Token numbers.  These MUST match the %token declarations in c0.y.
 * Token 0 is the end of file and 256 is the error token, both of which
 * are reserved by the parser.
 */

/* keywords */
#define	SIZEOF		257
#define	GOTO		258
#define	IF		259
#define	ELSE		260
#define	WHILE		261
#define	DO		262
#define	FOR		263
#define	RETURN		264
#define	BREAK		265
#define	CONTINUE	266
#define	SWITCH		267
#define	CASE		268
#define	DEFAULT		269
#define	CONST		270

/* storage class keywords; token SCLASS, value one of the SC_* codes */
#define	SCLASS		280
#define	SC_AUTO		1
#define	SC_STATIC	2
#define	SC_EXTERN	3
#define	SC_REGISTER	4
#define	SC_TYPEDEF	5
#define	SC_TAG		6
#define	SC_PARAM	7
#define	SC_GLOBAL	8		/* top-level definition (default) */

/* base type keywords; token TYPEKW, value a BT_* bit combination */
#define	TYPEKW		281

/* struct/union keywords; token SOU, value T_STRUCT or T_UNION */
#define	SOU		282

/* value tokens */
#define	TYPENAME	290
#define	IDENTIFIER	291
#define	INTEGER		292
#define	STRING		293

/*
 * Operator keyword classes.  Operators of equal precedence share one
 * token; the exact operator code is passed in yylval.
 */
#define	ASGN		300	/* =  +=  -=  *=  /=  %=  <<=  >>=  &=  |=  ^= */
#define	EQOP		301	/* ==  != */
#define	RELOP		302	/* <  >  <=  >= */
#define	SHIFT		303	/* <<  >> */
#define	ADDOP		304	/* +  - */
#define	MULOP		305	/* *  /  % */
#define	UNOP		306	/* !  ~ */
#define	INCOP		307	/* ++  -- */
#define	MBROP		308	/* .  -> */
#define	UMINUS		309	/* precedence marker only, never returned */
#define	ANDAND		310
#define	OROR		311

/* base type bits */
#define	BT_NONE		0x00
#define	BT_CHAR		0x01
#define	BT_SHORT	0x02
#define	BT_INT		0x04
#define	BT_LONG		0x08
#define	BT_VOID		0x10
#define	BT_UNSIGNED	0x20
#define	BT_SIGNED	0x40
#define	BT_CONST	0x80
#define	BT_MASK		0x1f

/* derived type codes (t_op) */
#define	T_STRUCT	0x0100
#define	T_UNION		0x0200
#define	T_PTR		0x0400
#define	T_ARY		0x0800
#define	T_FUNC		0x1000

/* tree operator codes; single characters are used where natural */
#define	O_EQ		0x80
#define	O_NE		0x81
#define	O_LE		0x82
#define	O_GE		0x83
#define	O_LS		0x84
#define	O_RS		0x85
#define	O_ANDAND	0x86
#define	O_OROR		0x87
#define	O_ADDA		0x88
#define	O_SUBA		0x89
#define	O_MULA		0x8a
#define	O_DIVA		0x8b
#define	O_MODA		0x8c
#define	O_ANDA		0x8d
#define	O_ORA		0x8e
#define	O_XORA		0x8f
#define	O_LSA		0x90
#define	O_RSA		0x91
#define	O_PREINC	0x92
#define	O_PREDEC	0x93
#define	O_POSTINC	0x94
#define	O_POSTDEC	0x95
#define	O_NAME		0x96
#define	O_CON		0x97
#define	O_STR		0x98
#define	O_CALL		0x99
#define	O_INDEX		0x9a
#define	O_CAST		0x9b
#define	O_COMMA		0x9c
#define	O_COND		0x9d
#define	O_MEMBER	0x9e
#define	O_ARROW		0x9f
#define	O_ADDR	0xa0
#define	O_ILIST	0xa1
#define	O_LCON	0xa2

struct type {
	int		 t_op;		/* BT_* bits or T_* code */
	struct type	*t_tp;		/* pointee / element / return type */
	int		 t_size;	/* T_ARY: element count; T_STRUCT: bytes */
	struct symb	*t_memb;	/* T_STRUCT/T_UNION: members;
					   T_FUNC: parameter name list */
	char		*t_tag;		/* T_STRUCT/T_UNION: tag, may be NULL */
};

struct symb {
	struct symb	*s_next;	/* scope chain */
	char		*s_name;
	struct type	*s_tp;
	int		 s_sc;		/* SC_*; struct/union members store
					   their byte offset here instead
					   (s_offs), as the two uses never
					   coincide */
};
#define	s_offs	s_sc

struct node {
	int		 n_op;
	struct type	*n_tp;
	struct node	*n_l, *n_r;
	int		 n_val;		/* O_CON: value; O_LCON: index into
					   lcons; O_STR: string index;
					   O_COND: else branch (a node *) */
};

/* values too large for an int live in this small pool (O_LCON nodes) */
extern long	 lcons[];

/* declarator parse tree */
#define	D_NAME		0
#define	D_PTR		1
#define	D_ARY		2
#define	D_FUNC		3

struct dcl {
	int		 d_op;
	struct dcl	*d_l;
	union {				/* by d_op, exactly one of these */
		char		*d_name;	/* D_NAME */
		struct symb	*d_params;	/* D_FUNC */
		struct node	*d_size;	/* D_ARY */
	} d_u;
};
#define	d_name	d_u.d_name
#define	d_params d_u.d_params
#define	d_size	d_u.d_size

/* declaration specifiers */
struct dspec {
	int		 p_bt;
	struct type	*p_tp;
};

/* statement records: the body lowered to jumps and branches (stmt.c) */
struct swcase {
	struct swcase	*next;
	int		 val;
	char		*lab;
};

struct srec {
	int		 kind;		/* S_* */
	union {
		struct {			/* S_EXPR, S_RET(V), S_LABEL,
					   S_JUMP, S_BR */
			struct node	*e;	/* expression / condition */
			char		*lab;	/* label / true label */
			char		*lf;	/* S_BR false label */
		} s;
		struct {			/* S_SW */
			struct node	*t;	/* the switch temporary */
			struct swcase	*cases;
			char		*lf;	/* end label (shared with s) */
			char		*dflt;	/* default label or NULL */
		} w;
	} u;
};
#define	r_e	u.s.e
#define	r_lab	u.s.lab
#define	r_lf	u.s.lf
#define	r_t	u.w.t
#define	r_cases	u.w.cases
#define	r_dflt	u.w.dflt

#define	S_EXPR		0
#define	S_RET		1
#define	S_RETV		2
#define	S_LABEL		3
#define	S_JUMP		4
#define	S_BR		5
#define	S_SW		6

extern int	 linenum;
extern int	 yylval;
extern int	 yychar;		 /* parser lookahead, for the lexer hack */
extern long	 numval;		 /* value of the last INTEGER */
extern int	 numbt;			 /* BT_* bits of the last INTEGER */
extern int	 nerrors;
extern char	 sdata[];		 /* the string bytes, for initializers */

int	 yylex ();
int	 lexpeek ();		 /* peek at the next token, unconsumed */
int	 yyparse ();
int	 yyerror ();
__dead void error ();
void	 typerr ();
void	 mini ();			 /* tiny formatter: %s %d %c %o */
void	 oputc (), oputs ();	 /* write bytes to a file descriptor */
int	 c0getc (), c0ungetc ();

/* type.c */
struct type	*mktype (), *btype (), *decay (), *usual ();
int		 fixbt (), isarith (), isptr (), isscalar (), compat (),
		 tysize ();

/* decl.c */
extern int	 nscope;
struct symb	*lookup (), *install (), *ginstall (), *insparam (),
		 *param1 (), *paramn ();
struct type	*curbase (), *su_begin (), *su_end (), *su_ref (),
		 *dcltype (), *tn_type ();
struct dcl	*dstar (), *dptrn (), *dchain (), *dname (), *dfunc (),
		 *dary ();
struct dspec	*tn_bt (), *tn_td (), *tn_su (), *tn_cat ();
char		*dclname ();
int		 curd_sc (), getlocals (), strnlen ();
void		 dcl_reset (), dclpool_reset (), sc_sclass (), sc_type (),
		 sc_const (), sc_su (), sc_td (), member (), bindparam (),
		 dclinst (), chkinit (), blkpush (), blkpop ();

/* expr.c */
struct node	*nname (), *ncon (), *nstr (), *nbina (), *nlog (),
		 *nasgn (), *ncond (), *nun (), *naddr (), *ninc (),
		 *nindex (), *nmember (), *ncall (), *ncast (), *ncomma (),
		 *nsize (), *ilist (), *exproper (), *nconst (), *nlocal ();
long		 fold ();
void		 expr_reset ();

/* stmt.c */
extern struct symb *curfunc;
void		 fdef_dcl (), fdefend (), loopbegin (), loopend (),
		 swbegin (), swend (), stmtx (), stmtcond (), stmtfor (),
		 stmtswitch (), stmtcase (), stmtdflt (), stmtret (),
		 stmtgoto (), stmtlabel (), stmtbrk (), stmtcont ();
void		 sif (), sifend (), selse (), sifendelse (), swhile (),
		 swhileend (), sdo (), sdoend (), sfor (), sforend (),
		 sswitch (), sswitchend ();

/* emit.c */
void	 emit_data (), emit_fdecl (), emit_static (), emit_func ();
char	*numstr ();		 /* append a decimal number to a string */

#endif /* FILE_C0_H */
