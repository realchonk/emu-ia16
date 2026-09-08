/* static char sccsid[] = "@(#)yylex.c	1.3 7/1/83"; */

#define isid(a)  ((fastab + COFF)[a] & IB)
#define IB 1
#define COFF 0

yylex() {
	static int ifdef=0;
	static char *op2[]={"||",  "&&" , ">>", "<<", ">=", "<=", "!=", "=="};
	static int  val2[]={OROR, ANDAND,  RS,   LS,   GE,   LE,   NE,   EQ};
	static char *opc="b\bt\tn\nf\fr\r\\\\";
	extern char fastab[];
	extern char *outp,*inp,*newp;
	extern walkifstack (), pushif (), popif ();
	register char savc, *s; char *skipbl(); int val;
	register char **p2;
	struct symtab {
		char *name;
		char *value;
	} *sp, *lookup();

	for (;;) {
		extern int passcom;		/* this crap makes #if's work */
		int opt_passcom = passcom;	/* even with -C option */
		passcom = 0;			/* (else comments make syntax errs) */
		newp = skipbl(newp);
		passcom = opt_passcom;		/* nb: lint uses -C so its useful! */

		if (*inp=='\n')
			return stop;	/* end of #if */

		savc = *newp;
		*newp = '\0';

		for (p2 = op2 + 8; --p2 >= op2; ) {	/* check 2-char ops */
			if (strcmp (*p2, inp) == 0) {
				val=val2[p2-op2];
				goto ret;
			}
		}

		s = "+-*/%<>&^|?:!~(),";	/* check 1-char ops */

		while (*s) {
		       	if (*s++ == *inp) {
				val = *--s;
				goto ret;
			}
		}

		if (*inp <= '9' && *inp >= '0') {/* a number */
			if (*inp=='0') {
				yylval = (inp[1]=='x' || inp[1]=='X') ?  tobinary(inp+2,16) : tobinary(inp+1,8);
			} else {
				yylval = tobinary(inp, 10);
			}
			val = number;
		} else if (isid (*inp)) {/* identifier */
			if (strcmp (inp, "defined") == 0) {
				ifdef = 1;
				pushif (0);
				val = DEFINED;
			} else {
				sp=lookup(inp, -1);
				if (ifdef != 0) {
					ifdef=0;
					popif ();
				}
				yylval = (sp->value == 0) ? 0 : 1;
				val = number;
			}
		} else if (*inp == '\'') {/* character constant */
			val = number;
			if (inp[1] == '\\') {/* escaped */
				char c;
				if (newp[-1] == '\'')
					newp[-1]='\0';
				s=opc;
				while (*s) {
					if (*s++!=inp[2]) {
						++s;
					} else {
						yylval= *s;
						goto ret;
					}
				}
				if (inp[2] <= '9' && inp[2] >= '0') {
					yylval = c = tobinary(inp + 2, 8);
				} else {
					yylval = inp[2];
				}
			} else {
				yylval=inp[1];
			}
		} else if (strcmp ("\\\n", inp) == 0) {
			*newp = savc;
			continue;
		} else {
			*newp = savc;
			pperror("Illegal character %c in preprocessor if", *inp);
			continue;
		}
ret:
		*newp = savc;
		outp = inp = newp;
		return val;
	}
}

tobinary(st, b)
char *st;
{
	int n = 0, c, t;
	char *s = st;

	while ((c = *s++) != '\0') {
		if (c >= '0' && c <= '9') {
			t = c - '0';
		} else if (b > 10 && c >= 'A' && c <= 'F') {
			t = c + 10 - 'A';
		} else if (b > 10 && c >= 'a' && c <= 'f') {
			t = c + 10 - 'a';
		} else {
			t = -1;
			if ((c == 'l' || c == 'L') && *s == '\0')
				continue;
			pperror ("illegal number %s", st);
		}
		if (t < 0)
			break;
		n = n * b + t;
	}
	return n;
}
