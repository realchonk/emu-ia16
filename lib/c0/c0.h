#ifndef FILE_C0_H
#define FILE_C0_H
#include <stdio.h>
#include <cdefs.h>

#define MAXIDENT	31

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
#define	O_ADDR		0xa0
#define	O_ILIST		0xa1

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
	int		 s_sc;		/* SC_* */
};

struct node {
	int		 n_op;
	struct type	*n_tp;
	struct node	*n_l, *n_r;
	long		 n_val;		/* O_CON: value; O_STR: string index;
					   O_MEMBER/O_ARROW: member name */
};

/* declarator parse tree */
#define	D_NAME		0
#define	D_PTR		1
#define	D_ARY		2
#define	D_FUNC		3

struct dcl {
	int		 d_op;
	struct dcl	*d_l;
	struct node	*d_size;	/* D_ARY: size expression or NULL */
	char		*d_name;	/* D_NAME */
	struct symb	*d_params;	/* D_FUNC: parameter names */
};

/* declaration specifiers */
struct dspec {
	int		 p_sc;
	int		 p_bt;
	struct type	*p_tp;
};

extern FILE	*fstr, *fir;
extern int	 linenum;
extern int	 yylval;
extern int	 yychar;		 /* parser lookahead, for the lexer hack */
extern long	 numval;		 /* value of the last INTEGER */
extern int	 numbt;			 /* BT_* bits of the last INTEGER */
extern int	 nerrors;

int	 yylex ();
int	 lexpeek ();		 /* peek at the next token, unconsumed */
int	 yyparse ();
int	 yyerror ();
__dead void error ();
void	 typerr ();

/* type.c */
struct type	*mktype (), *btype (), *decay (), *usual ();
int		 fixbt (), isarith (), isptr (), isscalar (), compat (),
		 tysize ();

/* decl.c */
extern int	 nscope;
struct symb	*lookup (), *install (), *insparam (), *param1 (),
		 *paramn ();
struct type	*curbase (), *su_begin (), *su_end (), *su_ref (),
		 *dcltype (), *tn_type ();
struct dcl	*dstar (), *dptrn (), *dchain (), *dname (), *dfunc (),
		 *dary ();
struct dspec	*tn_bt (), *tn_td (), *tn_su (), *tn_cat ();
char		*dclname ();
int		 curd_sc ();
void		 dcl_reset (), dclpool_reset (), sc_sclass (), sc_type (),
		 sc_const (), sc_su (), sc_td (), member (), bindparam (),
		 dclinst (), chkinit (), blkpush (), blkpop ();

/* expr.c */
struct node	*nname (), *ncon (), *nstr (), *nbina (), *nlog (),
		 *nasgn (), *ncond (), *nun (), *naddr (), *ninc (),
		 *nindex (), *nmember (), *ncall (), *ncast (), *ncomma (),
		 *nsize (), *ilist (), *exproper ();
long		 fold ();
void		 expr_reset ();

/* stmt.c */
void		 fdef_dcl (), fdefend (), loopbegin (), loopend (),
		 swbegin (), swend (), stmtx (), stmtcond (), stmtfor (),
		 stmtswitch (), stmtcase (), stmtdflt (), stmtret (),
		 stmtgoto (), stmtlabel (), stmtbrk (), stmtcont ();

#endif /* FILE_C0_H */
