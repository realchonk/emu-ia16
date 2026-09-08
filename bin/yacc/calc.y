{
#include <stdlib.h>
#include <stdio.h>
#include <ctype.h>

int ival;
%}

%union {
	int i;
};

%type <i> expr
%type <i> add
%type <i> mult
%type <i> atom

%token <i> INTEGER

%%

expr	: add		{ printf ("%d\n", $1); }
     	;

add	: add '+' mult	{ $$ = $1 + $3; }
    	| add '-' mult	{ $$ = $1 - $3; }
	| mult		{ $$ = $1; }
	;

mult	: mult '*' atom	{ $$ = $1 * $3; }
     	| mult '/' atom	{ $$ = $1 / $3; }
	| atom		{ $$ = $1; }
	;

atom	: INTEGER	{ $$ = ival; }
     	;

%%

yylex (void)
{
	int ch;

	ch = getchar ();
	while (isspace (ch))
		ch = getchar ();

	if (isdigit (ch)) {
		ival = 0;

		while (isdigit (ch)) {
			ival = ival * 10 + (ch - '0');
			ch = getchar ();
		}

		ungetc (ch, stdin);
		return INTEGER;
	} else switch (ch) {
	case '+':
	case '-':
	case '*':
	case '/':
		return ch;
	case EOF:
		return EOF;
	default:
		yyerror ("invalid input");
	}
}

yyerror (s)
char *s;
{
	fprintf (stderr, "error: %s\n", s);
	exit (1);
}

main ()
{
	yyparse ();
	return 0;
}
