/*
 * c0.y -- grammar for c0, the first pass of the C compiler.
 * The language is K&R C with structs and unions.  Semantic values are
 * pointers cast to int (they fit: both are 16 bits); YYSTYPE stays int.
 *
 * yacc reports 7 shift/reduce and 15 reduce/reduce conflicts; all are
 * expected and resolved correctly by its defaults:
 *   - the dangling `else` binds to the nearest `if` (shift);
 *   - after `(type)`, operators are taken as part of the cast operand
 *     (shift), so `(void *)-1` casts -1;
 *   - after `sizeof (type)`, the same operators end the type name
 *     (reduce by the %prec INCOP rule), so `sizeof (T) + 1` is
 *     (sizeof T) + 1;
 *   - the remaining reduce/reduce conflicts in the `sizeof (type)`
 *     state are equivalent: whether the `(type)` part reduces as a
 *     type-only cast or as the sizeof rule, nsize computes the size
 *     of the same type.
 */

%{
#include "c0.h"
%}

%token SIZEOF 257
%token GOTO 258
%token IF 259
%token ELSE 260
%token WHILE 261
%token DO 262
%token FOR 263
%token RETURN 264
%token BREAK 265
%token CONTINUE 266
%token SWITCH 267
%token CASE 268
%token DEFAULT 269
%token CONST 270

%token SCLASS 280
%token TYPEKW 281
%token SOU 282

%token TYPENAME 290
%token IDENTIFIER 291
%token INTEGER 292
%token STRING 293

%token ASGN 300
%token EQOP 301
%token RELOP 302
%token SHIFT 303
%token ADDOP 304
%token MULOP 305
%token UNOP 306
%token INCOP 307
%token MBROP 308
%token UMINUS 309
%token ANDAND 310
%token OROR 311

%right ASGN
%right '?' ':'
%left OROR
%left ANDAND
%left '|'
%left '^'
%left '&'
%left EQOP
%left RELOP
%left SHIFT
%left ADDOP
%left MULOP
%right UMINUS
%left INCOP

%%

prog	: xdefs
	| /* empty */
	;

xdefs	: xdef
	| xdefs xdef
	;

xdef	: datadecl
	| fdef
	| error ';'				{ dcl_reset (); }
	;

/*
 * External and local declarations.  At the outer level the type may be
 * omitted altogether (implicit int).
 */
datadecl: specs ';'				{ dcl_reset (); }
	| specs init_decl_list ';'		{ dcl_reset (); }
	| init_decl_list ';'			{ dcl_reset (); }
	;

bdatadecl: specs ';'				{ dcl_reset (); }
	| specs init_decl_list ';'		{ dcl_reset (); }
	;

specs	: spec
	| specs spec
	;

spec	: SCLASS				{ sc_sclass ($1); }
	| TYPEKW				{ sc_type ($1); }
	| CONST					{ sc_const (); }
	| TYPENAME				{ sc_td ((struct symb *) $1); }
	| su_spec				{ sc_su ((struct type *) $1); }
	;

su_spec	: SOU IDENTIFIER '{'			{ $$ = (int) su_begin ($1, (char *) $2); }
	  su_decls '}'			{ $$ = (int) su_end (); }
	| SOU IDENTIFIER '{' '}'		{ su_begin ($1, (char *) $2);
						  $$ = (int) su_end (); }
	| SOU '{'				{ $$ = (int) su_begin ($1, (char *) 0); }
	  su_decls '}'			{ $$ = (int) su_end (); }
	| SOU '{' '}'				{ su_begin ($1, (char *) 0);
						  $$ = (int) su_end (); }
	| SOU IDENTIFIER			{ $$ = (int) su_ref ($1, (char *) $2); }
	;

su_decls: su_decl
	| su_decls su_decl
	;

su_decl	: specs su_dlist ';'			{ dcl_reset (); }
	;

su_dlist: declarator				{ member ((struct dcl *) $1); }
	| su_dlist ',' declarator		{ member ((struct dcl *) $3); }
	;

declarator: pointer direct			{ $$ = (int) dchain ((struct dcl *) $1,
							  (struct dcl *) $2); }
	| direct				{ $$ = $1; }
	;

pointer	: MULOP				{ $$ = (int) dstar ($1); }
	| pointer MULOP			{ $$ = (int) dptrn ((struct dcl *) $1, $2); }
	;

direct	: IDENTIFIER			{ $$ = (int) dname ((char *) $1); }
	| '(' declarator ')'			{ $$ = $2; }
	| direct '(' ')'			{ $$ = (int) dfunc ((struct dcl *) $1,
							  (struct symb *) 0); }
	| direct '(' idlist ')'		{ $$ = (int) dfunc ((struct dcl *) $1,
							  (struct symb *) $3); }
	| direct '[' ']'			{ $$ = (int) dary ((struct dcl *) $1,
							 (struct node *) 0); }
	| direct '[' asgE ']'			{ $$ = (int) dary ((struct dcl *) $1,
							 (struct node *) $3); }
	;

idlist	: IDENTIFIER				{ $$ = (int) param1 ((char *) $1); }
	| idlist ',' IDENTIFIER		{ $$ = (int) paramn ((struct symb *) $1,
							  (char *) $3); }
	;

init_decl_list: init_declarator
	| init_decl_list ',' init_declarator
	;

init_declarator: declarator		{ dclinst ((struct dcl *) $1,
						 (struct node *) 0); }
	| declarator ASGN initializer	{ if ($2 != '=')
						  typerr ("expected '='");
					  dclinst ((struct dcl *) $1,
						   (struct node *) $3); }
	;

initializer: asgE
	| '{' init_list '}'			{ $$ = (int) ilist ((struct node *) $2); }
	| '{' init_list ',' '}'		{ $$ = (int) ilist ((struct node *) $2); }
	| '{' '}'				{ $$ = (int) ilist ((struct node *) 0); }
	;

init_list: initializer
	| init_list ',' initializer		{ $$ = (int) ncomma ((struct node *) $1,
							  (struct node *) $3); }
	;

/*
 * Function definitions, with K&R style parameter declarations between
 * the declarator and the body.  Only the function body may contain
 * declarations (at its start); nested compound statements hold
 * statements only, as in the earliest dialects.
 */
fdef	: fdefhead fbody			{ fdefend (); }
	| fdefhead pdecls fbody		{ fdefend (); }
	;

fdefhead: specs declarator			{ fdef_dcl ((struct dcl *) $2); }
	| declarator				{ fdef_dcl ((struct dcl *) $1); }
	;

pdecls	: pdecl
	| pdecls pdecl
	;

pdecl	: specs pdlist ';'			{ dcl_reset (); }
	;

pdlist	: declarator				{ bindparam ((struct dcl *) $1); }
	| pdlist ',' declarator		{ bindparam ((struct dcl *) $3); }
	;

/*
 * Type names for casts and sizeof; they keep their own specifier state
 * so as not to disturb a declaration in progress.
 */
type_name: tn_specs				{ $$ = (int) tn_type ((struct dspec *) $1,
							   (struct dcl *) 0); }
	| tn_specs abs_decl			{ $$ = (int) tn_type ((struct dspec *) $1,
							   (struct dcl *) $2); }
	;

tn_specs: tn_spec				{ $$ = $1; }
	| tn_specs tn_spec			{ $$ = (int) tn_cat ((struct dspec *) $1,
							  (struct dspec *) $2); }
	;

tn_spec	: TYPEKW				{ $$ = (int) tn_bt ($1); }
	| CONST					{ $$ = (int) tn_bt (BT_CONST); }
	| TYPENAME				{ $$ = (int) tn_td ((struct symb *) $1); }
	| su_spec				{ $$ = (int) tn_su ((struct type *) $1); }
	;

abs_decl: pointer				{ $$ = (int) dchain ((struct dcl *) $1,
							  (struct dcl *) 0); }
	| pointer direct_abs			{ $$ = (int) dchain ((struct dcl *) $1,
							  (struct dcl *) $2); }
	| direct_abs				{ $$ = $1; }
	;

direct_abs: '(' abs_decl ')'		{ $$ = $2; }
	| '(' ')'				{ $$ = (int) dfunc ((struct dcl *) 0,
							  (struct symb *) 0); }
	| direct_abs '(' ')'			{ $$ = (int) dfunc ((struct dcl *) $1,
							  (struct symb *) 0); }
	| direct_abs '[' ']'			{ $$ = (int) dary ((struct dcl *) $1,
							 (struct node *) 0); }
	| direct_abs '[' asgE ']'		{ $$ = (int) dary ((struct dcl *) $1,
							 (struct node *) $3); }
	;

/*
 * Statements.
 */
stmt	: e ';'					{ stmtx ((struct node *) $1); }
	| ';'
	| compound
	| IF ifcond stmt			{ sifend (); }
	| IF ifcond stmt ELSE		{ selse (); }
	  stmt				{ sifendelse (); }
	| WHILE '(' e ')'			{ stmtcond ((struct node *) $3);
						  swhile ((struct node *) $3);
						  loopbegin (); }
	  stmt				{ swhileend (); loopend (); }
	| DO					{ sdo (); loopbegin (); }
	  stmt WHILE '(' e ')' ';'		{ loopend ();
						  sdoend ((struct node *) $6); }
	| FOR '(' fe ';' fe ';' fe ')'	{ stmtfor ((struct node *) $3,
						 (struct node *) $5,
						 (struct node *) $7);
						  sfor ((struct node *) $3,
						 (struct node *) $5,
						 (struct node *) $7);
						  loopbegin (); }
	  stmt				{ sforend (); loopend (); }
	| SWITCH '(' e ')'			{ stmtswitch ((struct node *) $3);
						  sswitch ((struct node *) $3);
						  swbegin (); }
	  stmt				{ swend (); sswitchend (); }
	| BREAK ';'				{ stmtbrk (); }
	| CONTINUE ';'				{ stmtcont (); }
	| GOTO IDENTIFIER ';'			{ stmtgoto ((char *) $2); }
	| RETURN ';'				{ stmtret ((struct node *) 0); }
	| RETURN e ';'				{ stmtret ((struct node *) $2); }
	| IDENTIFIER ':'			{ stmtlabel ((char *) $1); } stmt
	| CASE asgE ':'			{ stmtcase ((struct node *) $2); } stmt
	| DEFAULT ':'			{ stmtdflt (); } stmt
	| error ';'
	;

/*
 * The if condition is factored out so that both if forms share one
 * action, which records the branch before the body is parsed.
 */
ifcond	: '(' e ')'				{ stmtcond ((struct node *) $2);
						  sif ((struct node *) $2); }
	;

fe	: /* empty */				{ $$ = 0; }
	| e
	;

/*
 * The body shares the parameter scope (pushed in fdef_dcl), so that
 * getlocals at fdefend sees the parameters and the declarations in
 * one list; the parameters come first.
 */
fbody	: '{' block '}'
	;

block	: dcls stmts
	| stmts
	| dcls
	| /* empty */
	;

dcls	: bdatadecl
	| dcls bdatadecl
	;

stmts	: stmt
	| stmts stmt
	;

/* K&R: any block may open with declarations, then statements */
compound: '{'				{ blkpush (); } block '}'	{ blkpop (); }
	;

/*
 * Expressions.  One production per precedence level; operators of a
 * class arrive with the exact code in the value.
 */
e	: e ',' asgE				{ $$ = (int) ncomma ((struct node *) $1,
							  (struct node *) $3); }
	| asgE
	;

asgE	: cast ASGN asgE			{ $$ = (int) nasgn ($2, (struct node *) $1,
							  (struct node *) $3); }
	| tern
	;

tern	: lor '?' e ':' asgE			{ $$ = (int) ncond ((struct node *) $1,
							  (struct node *) $3,
							  (struct node *) $5); }
	| lor
	;

lor	: lor OROR lan				{ $$ = (int) nlog (OROR, (struct node *) $1,
							  (struct node *) $3); }
	| lan
	;

lan	: lan ANDAND bor			{ $$ = (int) nlog (ANDAND, (struct node *) $1,
							  (struct node *) $3); }
	| bor
	;

bor	: bor '|' bxor				{ $$ = (int) nbina ('|', (struct node *) $1,
							  (struct node *) $3); }
	| bxor
	;

bxor	: bxor '^' band			{ $$ = (int) nbina ('^', (struct node *) $1,
							  (struct node *) $3); }
	| band
	;

band	: band '&' eq				{ $$ = (int) nbina ('&', (struct node *) $1,
							  (struct node *) $3); }
	| eq
	;

eq	: eq EQOP rel				{ $$ = (int) nbina ($2, (struct node *) $1,
							  (struct node *) $3); }
	| rel
	;

rel	: rel RELOP shift			{ $$ = (int) nbina ($2, (struct node *) $1,
							  (struct node *) $3); }
	| shift
	;

shift	: shift SHIFT add			{ $$ = (int) nbina ($2, (struct node *) $1,
							  (struct node *) $3); }
	| add
	;

add	: add ADDOP mul			{ $$ = (int) nbina ($2, (struct node *) $1,
							  (struct node *) $3); }
	| mul
	;

mul	: mul MULOP cast			{ $$ = (int) nbina ($2, (struct node *) $1,
							  (struct node *) $3); }
	| cast
	;

/*
 * A cast without an operand is only accepted by nsize: sizeof (type).
 */
cast	: '(' type_name ')' cast		{ $$ = (int) ncast ((struct type *) $2,
							  (struct node *) $4); }
	| '(' type_name ')'			{ $$ = (int) ncast ((struct type *) $2,
							  (struct node *) 0); }
	| unary
	;

unary	: UNOP cast				{ $$ = (int) nun ($1, (struct node *) $2); }
	| ADDOP cast %prec UMINUS		{ $$ = (int) nun ($1, (struct node *) $2); }
	| MULOP cast %prec UMINUS		{ $$ = (int) nun ($1, (struct node *) $2); }
	| INCOP cast %prec UMINUS		{ $$ = (int) ninc ($1,
							 (struct node *) $2, 1); }
	| '&' cast %prec UMINUS		{ $$ = (int) naddr ((struct node *) $2); }
	| SIZEOF '(' type_name ')' %prec INCOP	{ $$ = (int) nsize ((struct node *) 0,
							  (struct type *) $3); }
	| SIZEOF cast				{ $$ = (int) nsize ((struct node *) $2,
						 (struct type *) 0); }
	| postfix
	;

postfix	: primary
	| postfix '(' ')'			{ $$ = (int) ncall ((struct node *) $1,
							  (struct node *) 0); }
	| postfix '(' arglist ')'		{ $$ = (int) ncall ((struct node *) $1,
							  (struct node *) $3); }
	| postfix '[' e ']'			{ $$ = (int) nindex ((struct node *) $1,
							  (struct node *) $3); }
	| postfix MBROP IDENTIFIER		{ $$ = (int) nmember ((struct node *) $1, $2,
							  (char *) $3); }
	| postfix INCOP			{ $$ = (int) ninc ($2, (struct node *) $1, 0); }
	;

arglist	: asgE
	| arglist ',' asgE			{ $$ = (int) ncomma ((struct node *) $1,
							  (struct node *) $3); }
	;

primary	: IDENTIFIER				{ $$ = (int) nname ((char *) $1); }
	| INTEGER				{ $$ = (int) ncon (); }
	| STRING				{ $$ = (int) nstr ($1); }
	| primary STRING			{ $$ = (int) nstrcat ((struct node *) $1,
							  $2); }
	| '(' e ')'				{ $$ = $2; }
	;
