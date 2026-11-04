
# line 21 "c0.y"
#include "c0.h"
# define SIZEOF 257
# define GOTO 258
# define IF 259
# define ELSE 260
# define WHILE 261
# define DO 262
# define FOR 263
# define RETURN 264
# define BREAK 265
# define CONTINUE 266
# define SWITCH 267
# define CASE 268
# define DEFAULT 269
# define CONST 270
# define FAR 271
# define SCLASS 280
# define TYPEKW 281
# define SOU 282
# define TYPENAME 290
# define IDENTIFIER 291
# define INTEGER 292
# define STRING 293
# define ASGN 300
# define EQOP 301
# define RELOP 302
# define SHIFT 303
# define ADDOP 304
# define MULOP 305
# define UNOP 306
# define INCOP 307
# define MBROP 308
# define UMINUS 309
# define ANDAND 310
# define OROR 311
#define yyclearin yychar = -1
#define yyerrok yyerrflag = 0
extern int yychar;
extern short yyerrflag;
#ifndef YYMAXDEPTH
#define YYMAXDEPTH 150
#endif
#ifndef YYSTYPE
#define YYSTYPE int
#endif
YYSTYPE yylval, yyval;
# define YYERRCODE 256
short yyexca[] ={
-1, 1,
	0, -1,
	-2, 0,
-1, 12,
	59, 47,
	44, 47,
	-2, 58,
-1, 30,
	59, 47,
	44, 47,
	-2, 57,
-1, 239,
	304, 147,
	305, 147,
	38, 147,
	-2, 140,
	};
# define YYNPROD 163
# define YYLAST 730
short yyact[]={

  85, 148,  78, 161, 170, 169, 168, 167, 166,  24,
  46, 138,  38, 162, 106, 222, 209,  44, 245,  35,
 157,  59, 130, 230, 224, 227, 102, 180,  98, 187,
  10,  85,  99,  78,  57,   7,  97,   7,  29,  24,
  95,  96,  88,  24,  37,  94, 142,  93, 221,  18,
  90, 236,  85, 139,  78,  52, 114, 163, 111, 164,
 121, 178, 257,  41, 121,  32, 172, 266,  29,  37,
  85, 158,  78,  59,  85, 121,  78, 256, 261, 191,
 181, 171,  32, 231,  32,  75, 121, 190,  29, 136,
 120,  85, 129,  78, 205, 193,  85,  47,  78,  31,
 238,  54, 128,  26,   8, 135, 133, 264, 121, 242,
 121,  55,  28,  85,  42,  78, 104, 131, 243, 240,
 232, 229, 241, 121, 121, 267, 239, 228, 147, 220,
 121, 175, 197,  85, 113,  78, 250,  75, 198, 165,
 176, 121,  85, 177, 155, 127, 126, 124, 123,  36,
  85,  20,  78,   3, 206, 115,  25,  89,  87, 104,
  79, 174, 119,  15,  16, 116,  15,  16, 109,  15,
  16,  39,  80,  13,  14,  21,  13,  14,  21,  13,
  14,  21,  24,  17,  51,  43,  17,  33, 200,  17,
  78, 147, 202, 195,  77,  24, 137, 216,  24,  53,
 194, 118, 217, 234, 215, 147, 213, 223, 214, 204,
 212, 211, 210,  11,  27, 226, 192, 252,  73,  86,
  68,  61,  50,  62,  63,  64,  69,  66,  67,  65,
  71,  72,  15,  16,  24, 104,  24, 268, 125, 249,
 248, 122,  13,  14,  21,  60,  48, 141, 244, 149,
  86, 100,  17,  70,  91,  92,  34,  29, 226,   9,
  23, 251, 173, 107, 108,  19,  82,  83,  81,  84,
  73,  86,  68,  61,  22,  62,  63,  64,  69,  66,
  67,  65,  71,  72, 105,  91,  92, 160, 159,  86,
  23, 265, 246,  86,  23, 225, 112,  82,  83,  81,
  84, 179, 144, 145,  22,  70,  91,  92,  40,   5,
  86,   4,   2, 143,  21,  86,   1,   0,  82,  83,
  81,  84, 146, 105,  91,  92,   0, 105,  91,  92,
   0,   0,  86,   0,   0,  56,  82,  83,  81,  84,
  82,  83,  81,  84, 105,  91,  92,   0,   0, 105,
  91,  92,  86,   0,   0,   0,   0,  82,  83,  81,
  84,  86,  82,  83,  81,  84, 105,  91,  92,  86,
   0,   0,   0,   0,   0,   0,   0,   0,   0,  82,
  83,  81,  84,  58,   0,   0, 105,  91,  92,   0,
 117,   0,   0,   0,   0, 105,  91,  92,   0,  82,
  83,  81,  84, 105,  91,  92,   0,  86,  82,  83,
  81,  84,  15,  16,   6,   0,  82,  83,  81,  84,
   0,   0,  13,  14,  21,  15,  16,   0,  15,  16,
  76,   0,  17,  23,   0,  13,  14,  21,  13,  14,
  21, 105,  91,  92,   0,  17,  23,  22,  17,  23,
  74, 117,   0, 132,   0,   0,  81,  84, 183,   0,
  22, 186, 140,  22,  15,  16, 144, 145,   0,   0,
   0,  12,   0,   0,  13,  14,  21, 143,  21,  30,
   0,   0,   0,   0,  17,  23, 146,  23,   0, 103,
   0,   0,   0, 110,   0,   0,  45,   0,   0,  22,
   0,  22,   0,   0,  49,   0,   0, 184, 185, 101,
 188, 189, 150, 151, 152, 153, 154, 156,   0,   0,
   0,   0, 134,   0,   0,   0,   0,   0, 233,  49,
 235,   0, 201,   0,   0,   0,   0,   0,   0, 140,
   0,   0, 208,   0,   0,   0,   0,   0,   0,   0,
   0,   0,   0,   0,   0, 103,   0,   0,   0,   0,
   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,
 253,   0, 182,   0,   0,   0,   0,   0,   0,   0,
 203,   0,   0,   0, 258, 259,   0,   0, 262, 196,
   0,   0, 203,   0, 203, 203, 203, 203, 203, 203,
 203, 218,   0,   0, 269,   0,   0,   0, 207,   0,
   0,   0,   0, 199,   0, 188,   0,   0,   0,   0,
   0,   0,   0,   0,   0,   0,   0,   0, 237,   0,
   0,   0,   0,   0, 260,   0,   0,   0,   0,   0,
   0,   0,   0,   0, 219, 188,   0,   0,   0,   0,
   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,
   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,
 237,   0, 103,   0,   0,   0,   0,   0,   0,   0,
   0,   0,   0,   0,   0,   0,   0,   0,   0, 254,
   0,   0, 255,   0,   0,   0,   0,   0, 247,   0,
   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,
   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,
   0,   0,   0,   0,   0,   0,   0,   0,   0, 263 };
short yypact[]={

 158,-1000, 158,-1000,-1000,-1000,  44, 155,  40,-104,
-1000,-1000,-288,-1000,-1000,-1000,-1000,-1000,-1000,   3,
  23,-106,-1000,-1000, -31,-1000,-1000,-1000,  38,-1000,
-288,-1000,  -1,-1000,-104, -38,-1000, 194, 112,  23,
-1000, -27,  75, -65,-1000,  93,-1000,-1000,-1000,-288,
-1000,-1000, -69, -38,  14,-1000,-1000, 142,  31,-1000,
-1000, 108, 107,-1000, 106, 105,  43,  33,-269,  58,
  48,  95,  47,  30,-1000,-1000,-289,-1000,  32,-1000,
 -62,  95,  95,  95,  95,  95, 104, -20,-307,-280,
 -67,-1000,-1000, -35, 101,-293,-295,-297,-299,-301,
  22,-1000,-1000,-1000,  36,-1000,-1000,  99,-1000,-1000,
 -32,-1000,-1000,-1000,-1000,  14,-1000,-1000,-1000,  21,
-1000,  95,  14,  95,  95,  14,  95,  95,-1000,-1000,
  28,-1000,  20,-1000,  37,-1000,-1000, -38,  95,  91,
  97, 196,-1000,-1000,-1000,-1000,-1000,-1000,  95,  95,
-1000,-1000,-1000,-1000,-1000,  32,-1000,  53,  95,-275,
-1000,  95,-1000,  95,  95,  95,  95,  95,  95,  95,
  95,-1000,  -1,   4,-1000,-1000,-1000,-276,-1000,-1000,
-101,-1000,-1000,-235,  86,  80,-238,  24,  64,  79,
-1000,-1000,  14,-1000,  14, -74,-1000,  95,-1000,-1000,
-1000,  42,-307,-1000,  85,-1000,  78,-1000,  16,-1000,
 -67, -35, 101,-293,-295,-297,-299,-301,-1000,-1000,
-1000,  -7,-1000,-107,-1000,-1000, 194,-1000,-1000,-1000,
  96,  95,-1000,-1000,  14,-1000,-1000,-1000,  95, 150,
-1000,  95,-1000,-1000,-1000,-1000,  18,-1000,  14,  14,
  95,  19,  14,-1000,-1000,-1000,-1000,  -1,-1000,-1000,
  66,  95,-1000,-1000,   8,  84,-1000,-1000,  14,-1000 };
short yypgo[]={

   0, 316, 312, 153, 311, 309,  34, 104, 111,  30,
  49, 301,  27, 296, 295, 292, 471, 265, 151, 263,
 450, 213,  26, 262, 259, 187, 256, 149, 251,  53,
 247,  46, 335, 383, 245, 241, 240, 239, 238,  29,
 237, 217, 216, 203, 200,  55, 199, 101, 196, 430,
 194, 172,  42,  50,  47,  45,  40,  41,  36,  28,
  32, 160, 158, 157, 154 };
short yyr1[]={

   0,   1,   1,   2,   2,   3,   3,   3,   4,   4,
   4,   8,   8,   6,   6,   9,   9,   9,   9,   9,
   9,  11,  10,  13,  10,  10,  12,  12,  14,  15,
  15,  16,  16,  16,  17,  17,  18,  18,  18,  18,
  18,  18,  18,  19,  19,   7,   7,  21,  21,  22,
  22,  22,  22,  23,  23,   5,   5,  24,  24,  26,
  26,  27,  28,  28,  29,  29,  30,  30,  31,  31,
  31,  31,  31,  32,  32,  32,  32,  36,  32,  37,
  32,  38,  32,  40,  32,  41,  32,  32,  32,  32,
  32,  32,  42,  32,  43,  32,  44,  32,  32,  35,
  39,  39,  25,  45,  45,  45,  45,  46,  46,  47,
  47,  48,  34,  33,  33,  20,  20,  50,  50,  51,
  51,  52,  52,  53,  53,  54,  54,  55,  55,  56,
  56,  57,  57,  58,  58,  59,  59,  60,  60,  49,
  49,  49,  61,  61,  61,  61,  61,  61,  61,  61,
  62,  62,  62,  62,  62,  62,  64,  64,  63,  63,
  63,  63,  63 };
short yyr2[]={

   0,   1,   0,   1,   2,   1,   1,   2,   2,   3,
   2,   2,   3,   1,   2,   1,   1,   1,   1,   1,
   1,   0,   6,   0,   5,   2,   0,   2,   3,   1,
   3,   2,   1,   1,   1,   2,   1,   3,   2,   3,
   4,   3,   4,   1,   3,   1,   3,   1,   3,   1,
   3,   4,   2,   1,   3,   2,   3,   2,   1,   1,
   2,   3,   1,   3,   1,   2,   1,   2,   1,   1,
   1,   1,   1,   2,   1,   1,   3,   0,   6,   0,
   6,   0,   8,   0,  10,   0,   6,   2,   2,   3,
   2,   3,   0,   4,   0,   5,   0,   4,   2,   3,
   0,   1,   3,   2,   1,   1,   0,   1,   2,   1,
   2,   0,   4,   3,   1,   3,   1,   5,   1,   3,
   1,   3,   1,   3,   1,   3,   1,   3,   1,   3,
   1,   3,   1,   3,   1,   3,   1,   3,   1,   4,
   3,   1,   2,   2,   2,   2,   2,   4,   2,   1,
   1,   3,   4,   4,   3,   2,   1,   3,   1,   1,
   1,   2,   3 };
short yychk[]={

-1000,  -1,  -2,  -3,  -4,  -5, 256,  -6,  -7, -24,
  -9, -21, -16, 280, 281, 270, 271, 290, -10, -17,
 -18, 282, 305, 291,  40,  -3,  59,  59,  -7,  -9,
 -16,  59,  44, -25, -26, 123, -27,  -6, 300, -18,
 305,  40,  91, 291, 123, -16,  41,  59, -21, -16,
 -25, -27, -45, -46, -47,  -8, -32,  -6, -33,  59,
 -34, 259, 261, 262, 263, 267, 265, 266, 258, 264,
 291, 268, 269, 256, -20, 123, -49, -50,  40, -61,
 -51, 306, 304, 305, 307,  38, 257, -62, -52, -63,
 -53, 292, 293, -54, -55, -56, -57, -58, -59, -60,
 -28, -16, -22, -20, 123, 291,  41, -19, 291,  93,
 -20, 123, -13,  41, 125, -47,  -8, -32,  59,  -7,
  59,  44, -35,  40,  40, -38,  40,  40,  59,  59,
 291,  59, -33,  58, -20,  58,  59, -48, 300, -29,
 -33, -30, -31, 281, 270, 271, 290, -10,  63, 311,
 -49, -49, -49, -49, -49,  40, -49,  40,  91, 308,
 307, 310, 293, 124,  94,  38, 301, 302, 303, 304,
 305,  59,  44, -23, 125, -22,  41,  44,  93, -11,
 -12,  59, -20, -32, -33, -33, -32, -39, -33, -33,
  59,  59, -42,  58, -44, -45, -20,  41,  41, -16,
 -31, -33, -52, -49, -29,  41, -64, -20, -33, 291,
 -53, -54, -55, -56, -57, -58, -59, -60, -49, -16,
 125,  44, 291, -12, 125, -14,  -6, 260,  41,  41,
 261,  59,  41, -32, -43, -32, 125, -49,  58,  41,
  41,  44,  93, 125, -22, 125, -15, -16, -36, -37,
  40, -39, -41, -32, -20, -20,  59,  44, -32, -32,
 -33,  59, -32, -16,  41, -39,  59,  41, -40, -32 };
short yydef[]={

   2,  -2,   1,   3,   5,   6,   0,   0,   0,   0,
  13,  45,  -2,  15,  16,  17,  18,  19,  20,  33,
  32,   0,  34,  36,   0,   4,   7,   8,   0,  14,
  -2,  10,   0,  55,   0, 106,  59,   0,   0,  31,
  35,   0,   0,  25,  23,   0,  38,   9,  46,  47,
  56,  60,   0, 105, 104, 107, 109,   0,   0,  74,
  75,   0,   0,  81,   0,   0,   0,   0,   0,   0,
 158,   0,   0,   0, 114, 111, 138, 116,   0, 141,
 118,   0,   0,   0,   0,   0,   0, 149, 120, 150,
 122, 159, 160, 124, 126, 128, 130, 132, 134, 136,
   0,  62,  48,  49,   0, 158,  39,   0,  43,  41,
   0,  21,  26,  37, 102, 103, 108, 110,  11,   0,
  73,   0,   0,   0,   0,   0, 100,   0,  87,  88,
   0,  90,   0,  92,   0,  96,  98, 106,   0,   0,
   0,  64,  66,  68,  69,  70,  71,  72,   0,   0,
 142, 143, 144, 145, 146,   0, 148,   0,   0,   0,
 155,   0, 161,   0,   0,   0,   0,   0,   0,   0,
   0,  61,   0,   0,  52,  53,  40,   0,  42,  26,
   0,  12, 113,  76,   0,   0,   0,   0, 101,   0,
  89,  91,   0,  94,   0,   0, 115, 140, 162,  65,
  67,   0, 119, 138,   0, 151,   0, 156,   0, 154,
 121, 123, 125, 127, 129, 131, 133, 135, 137,  63,
  50,   0,  44,   0,  24,  27,   0,  77,  99,  79,
   0, 100,  85,  93,   0,  97, 112, 139,   0,  -2,
 152,   0, 153,  51,  54,  22,   0,  29,   0,   0,
   0,   0,   0,  95, 117, 157,  28,   0,  78,  80,
   0, 100,  86,  30,   0,   0,  82,  83,   0,  84 };
# define YYFLAG -1000
# define YYERROR goto yyerrlab
# define YYACCEPT return(0)
# define YYABORT return(1)

/*	parser for yacc output	*/

#ifdef YYDEBUG
int yydebug = 0; /* 1 for debugging */
#endif
YYSTYPE yyv[YYMAXDEPTH]; /* where the values are stored */
int yychar = -1; /* current input token number */
int yynerrs = 0;  /* number of errors */
short yyerrflag = 0;  /* error recovery flag */

int yyparse() {

	short yys[YYMAXDEPTH];
	short yyj, yym;
	register YYSTYPE *yypvt;
	register short yystate, *yyps, yyn;
	register YYSTYPE *yypv;
	register short *yyxi;

	yystate = 0;
	yychar = -1;
	yynerrs = 0;
	yyerrflag = 0;
	yyps= &yys[-1];
	yypv= &yyv[-1];

 yystack:    /* put a state and value onto the stack */

#ifdef YYDEBUG
	if( yydebug  ) printf( "state %d, char 0%o\n", yystate, yychar );
#endif
		if( ++yyps> &yys[YYMAXDEPTH] ) { yyerror( "yacc stack overflow" ); return(1); }
		*yyps = yystate;
		++yypv;
		*yypv = yyval;

 yynewstate:

	yyn = yypact[yystate];

	if( yyn<= YYFLAG ) goto yydefault; /* simple state */

	if( yychar<0 ) if( (yychar=yylex())<0 ) yychar=0;
	if( (yyn += yychar)<0 || yyn >= YYLAST ) goto yydefault;

	if( yychk[ yyn=yyact[ yyn ] ] == yychar ){ /* valid shift */
		yychar = -1;
		yyval = yylval;
		yystate = yyn;
		if( yyerrflag > 0 ) --yyerrflag;
		goto yystack;
		}

 yydefault:
	/* default state action */

	if( (yyn=yydef[yystate]) == -2 ) {
		if( yychar<0 ) if( (yychar=yylex())<0 ) yychar = 0;
		/* look through exception table */

		for( yyxi=yyexca; (*yyxi!= (-1)) || (yyxi[1]!=yystate) ; yyxi += 2 ) ; /* VOID */

		while( *(yyxi+=2) >= 0 ){
			if( *yyxi == yychar ) break;
			}
		if( (yyn = yyxi[1]) < 0 ) return(0);   /* accept */
		}

	if( yyn == 0 ){ /* error */
		/* error ... attempt to resume parsing */

		switch( yyerrflag ){

		case 0:   /* brand new error */

			yyerror( "syntax error" );
		if( 0 ) goto yyerrlab; /* pacify the compiler: YYERROR may not be used */
		yyerrlab:
			++yynerrs;

		case 1:
		case 2: /* incompletely recovered error ... try again */

			yyerrflag = 3;

			/* find a state where "error" is a legal shift action */

			while ( yyps >= yys ) {
			   yyn = yypact[*yyps] + YYERRCODE;
			   if( yyn>= 0 && yyn < YYLAST && yychk[yyact[yyn]] == YYERRCODE ){
			      yystate = yyact[yyn];  /* simulate a shift of "error" */
			      goto yystack;
			      }
			   yyn = yypact[*yyps];

			   /* the current yyps has no shift onn "error", pop stack */

#ifdef YYDEBUG
			   if( yydebug ) printf( "error recovery pops state %d, uncovers %d\n", *yyps, yyps[-1] );
#endif
			   --yyps;
			   --yypv;
			   }

			/* there is no state on the stack with an error shift ... abort */

	yyabort:
			return(1);


		case 3:  /* no shift yet; clobber input char */

#ifdef YYDEBUG
			if( yydebug ) printf( "error recovery discards char %d\n", yychar );
#endif

			if( yychar == 0 ) goto yyabort; /* don't discard EOF, quit */
			yychar = -1;
			goto yynewstate;   /* try again in the same state */

			}

		}

	/* reduction by production yyn */

#ifdef YYDEBUG
		if( yydebug ) printf("reduce %d\n",yyn);
#endif
		yyps -= yyr2[yyn];
		yypvt = yypv;
		yypv -= yyr2[yyn];
		yyval = yypv[1];
		yym=yyn;
			/* consult goto table to find next state */
		yyn = yyr1[yyn];
		yyj = yypgo[yyn] + *yyps + 1;
		if( yyj>=YYLAST || yychk[ yystate = yyact[yyj] ] != -yyn ) yystate = yyact[yypgo[yyn]];
		switch(yym){
			
case 7:
# line 89 "c0.y"
{ dcl_reset (); } break;
case 8:
# line 97 "c0.y"
{ dcl_reset (); } break;
case 9:
# line 98 "c0.y"
{ dcl_reset (); } break;
case 10:
# line 99 "c0.y"
{ dcl_reset (); } break;
case 11:
# line 102 "c0.y"
{ dcl_reset (); } break;
case 12:
# line 103 "c0.y"
{ dcl_reset (); } break;
case 15:
# line 110 "c0.y"
{ sc_sclass (yypvt[-0]); } break;
case 16:
# line 111 "c0.y"
{ sc_type (yypvt[-0]); } break;
case 17:
# line 112 "c0.y"
{ sc_const (); } break;
case 18:
# line 113 "c0.y"
{ sc_far (); } break;
case 19:
# line 114 "c0.y"
{ sc_td ((struct symb *) yypvt[-0]); } break;
case 20:
# line 115 "c0.y"
{ sc_su ((struct type *) yypvt[-0]); } break;
case 21:
# line 118 "c0.y"
{ yyval = (int) su_begin (yypvt[-2], yypvt[-1]); } break;
case 22:
# line 119 "c0.y"
{ yyval = (int) su_end (); } break;
case 23:
# line 120 "c0.y"
{ yyval = (int) su_begin (yypvt[-1], 0); } break;
case 24:
# line 121 "c0.y"
{ yyval = (int) su_end (); } break;
case 25:
# line 122 "c0.y"
{ yyval = (int) su_ref (yypvt[-1], yypvt[-0]); } break;
case 28:
# line 129 "c0.y"
{ dcl_reset (); } break;
case 29:
# line 132 "c0.y"
{ member ((struct dcl *) yypvt[-0]); } break;
case 30:
# line 133 "c0.y"
{ member ((struct dcl *) yypvt[-0]); } break;
case 31:
# line 142 "c0.y"
{ yyval = (int) dchain ((struct dcl *) yypvt[-1],
							  (struct dcl *) yypvt[-0]); } break;
case 32:
# line 144 "c0.y"
{ yyval = yypvt[-0]; } break;
case 33:
# line 145 "c0.y"
{ yyval = (int) dchain ((struct dcl *) yypvt[-0],
							  (struct dcl *) 0); } break;
case 34:
# line 149 "c0.y"
{ yyval = (int) dstar (yypvt[-0]); } break;
case 35:
# line 150 "c0.y"
{ yyval = (int) dptrn ((struct dcl *) yypvt[-1], yypvt[-0]); } break;
case 36:
# line 153 "c0.y"
{ yyval = (int) dname (yypvt[-0]); } break;
case 37:
# line 154 "c0.y"
{ yyval = yypvt[-1]; } break;
case 38:
# line 155 "c0.y"
{ yyval = (int) dfunc ((struct dcl *) 0,
							  (struct symb *) 0); } break;
case 39:
# line 157 "c0.y"
{ yyval = (int) dfunc ((struct dcl *) yypvt[-2],
							  (struct symb *) 0); } break;
case 40:
# line 159 "c0.y"
{ yyval = (int) dfunc ((struct dcl *) yypvt[-3],
							  (struct symb *) yypvt[-1]); } break;
case 41:
# line 161 "c0.y"
{ yyval = (int) dary ((struct dcl *) yypvt[-2],
							 (struct node *) 0); } break;
case 42:
# line 163 "c0.y"
{ yyval = (int) dary ((struct dcl *) yypvt[-3],
							 (struct node *) yypvt[-1]); } break;
case 43:
# line 167 "c0.y"
{ yyval = (int) param1 (yypvt[-0]); } break;
case 44:
# line 168 "c0.y"
{ yyval = (int) paramn ((struct symb *) yypvt[-2],
							  yypvt[-0]); } break;
case 47:
# line 176 "c0.y"
{ dclinst ((struct dcl *) yypvt[-0],
						 (struct node *) 0); } break;
case 48:
# line 178 "c0.y"
{ if (yypvt[-1] != '=')
						  typerr ("expected '='");
					  dclinst ((struct dcl *) yypvt[-2],
						   (struct node *) yypvt[-0]); } break;
case 50:
# line 185 "c0.y"
{ yyval = (int) ilist ((struct node *) yypvt[-1]); } break;
case 51:
# line 186 "c0.y"
{ yyval = (int) ilist ((struct node *) yypvt[-2]); } break;
case 52:
# line 187 "c0.y"
{ yyval = (int) ilist ((struct node *) 0); } break;
case 54:
# line 191 "c0.y"
{ yyval = (int) ncomma ((struct node *) yypvt[-2],
							  (struct node *) yypvt[-0]); } break;
case 55:
# line 201 "c0.y"
{ fdefend (); } break;
case 56:
# line 202 "c0.y"
{ fdefend (); } break;
case 57:
# line 205 "c0.y"
{ fdef_dcl ((struct dcl *) yypvt[-0]); } break;
case 58:
# line 206 "c0.y"
{ fdef_dcl ((struct dcl *) yypvt[-0]); } break;
case 61:
# line 213 "c0.y"
{ dcl_reset (); } break;
case 62:
# line 216 "c0.y"
{ bindparam ((struct dcl *) yypvt[-0]); } break;
case 63:
# line 217 "c0.y"
{ bindparam ((struct dcl *) yypvt[-0]); } break;
case 64:
# line 224 "c0.y"
{ yyval = (int) tn_type ((struct dspec *) yypvt[-0],
							   (struct dcl *) 0); } break;
case 65:
# line 226 "c0.y"
{ yyval = (int) tn_type ((struct dspec *) yypvt[-1],
							   (struct dcl *) yypvt[-0]); } break;
case 66:
# line 230 "c0.y"
{ yyval = yypvt[-0]; } break;
case 67:
# line 231 "c0.y"
{ yyval = (int) tn_cat ((struct dspec *) yypvt[-1],
							  (struct dspec *) yypvt[-0]); } break;
case 68:
# line 235 "c0.y"
{ yyval = (int) tn_bt (yypvt[-0]); } break;
case 69:
# line 236 "c0.y"
{ yyval = (int) tn_bt (BT_CONST); } break;
case 70:
# line 237 "c0.y"
{ yyval = (int) tn_far (); } break;
case 71:
# line 238 "c0.y"
{ yyval = (int) tn_td ((struct symb *) yypvt[-0]); } break;
case 72:
# line 239 "c0.y"
{ yyval = (int) tn_su ((struct type *) yypvt[-0]); } break;
case 73:
# line 245 "c0.y"
{ stmtx ((struct node *) yypvt[-1]); } break;
case 76:
# line 248 "c0.y"
{ sifend (); } break;
case 77:
# line 249 "c0.y"
{ selse (); } break;
case 78:
# line 250 "c0.y"
{ sifendelse (); } break;
case 79:
# line 251 "c0.y"
{ stmtcond ((struct node *) yypvt[-1]);
						  swhile ((struct node *) yypvt[-1]);
						  loopbegin (); } break;
case 80:
# line 254 "c0.y"
{ swhileend (); loopend (); } break;
case 81:
# line 255 "c0.y"
{ sdo (); loopbegin (); } break;
case 82:
# line 256 "c0.y"
{ loopend ();
						  sdoend ((struct node *) yypvt[-2]); } break;
case 83:
# line 258 "c0.y"
{ stmtfor ((struct node *) yypvt[-5],
						 (struct node *) yypvt[-3],
						 (struct node *) yypvt[-1]);
						  sfor ((struct node *) yypvt[-5],
						 (struct node *) yypvt[-3],
						 (struct node *) yypvt[-1]);
						  loopbegin (); } break;
case 84:
# line 265 "c0.y"
{ sforend (); loopend (); } break;
case 85:
# line 266 "c0.y"
{ stmtswitch ((struct node *) yypvt[-1]);
						  sswitch ((struct node *) yypvt[-1]);
						  swbegin (); } break;
case 86:
# line 269 "c0.y"
{ swend (); sswitchend (); } break;
case 87:
# line 270 "c0.y"
{ stmtbrk (); } break;
case 88:
# line 271 "c0.y"
{ stmtcont (); } break;
case 89:
# line 272 "c0.y"
{ stmtgoto (yypvt[-1]); } break;
case 90:
# line 273 "c0.y"
{ stmtret ((struct node *) 0); } break;
case 91:
# line 274 "c0.y"
{ stmtret ((struct node *) yypvt[-1]); } break;
case 92:
# line 275 "c0.y"
{ stmtlabel (yypvt[-1]); } break;
case 94:
# line 276 "c0.y"
{ stmtcase ((struct node *) yypvt[-1]); } break;
case 96:
# line 277 "c0.y"
{ stmtdflt (); } break;
case 99:
# line 285 "c0.y"
{ stmtcond ((struct node *) yypvt[-1]);
						  sif ((struct node *) yypvt[-1]); } break;
case 100:
# line 289 "c0.y"
{ yyval = 0; } break;
case 111:
# line 316 "c0.y"
{ blkpush (); } break;
case 112:
# line 316 "c0.y"
{ blkpop (); } break;
case 113:
# line 323 "c0.y"
{ yyval = (int) ncomma ((struct node *) yypvt[-2],
							  (struct node *) yypvt[-0]); } break;
case 115:
# line 328 "c0.y"
{ yyval = (int) nasgn (yypvt[-1], (struct node *) yypvt[-2],
							  (struct node *) yypvt[-0]); } break;
case 117:
# line 333 "c0.y"
{ yyval = (int) ncond ((struct node *) yypvt[-4],
							  (struct node *) yypvt[-2],
							  (struct node *) yypvt[-0]); } break;
case 119:
# line 339 "c0.y"
{ yyval = (int) nlog (OROR, (struct node *) yypvt[-2],
							  (struct node *) yypvt[-0]); } break;
case 121:
# line 344 "c0.y"
{ yyval = (int) nlog (ANDAND, (struct node *) yypvt[-2],
							  (struct node *) yypvt[-0]); } break;
case 123:
# line 349 "c0.y"
{ yyval = (int) nbina ('|', (struct node *) yypvt[-2],
							  (struct node *) yypvt[-0]); } break;
case 125:
# line 354 "c0.y"
{ yyval = (int) nbina ('^', (struct node *) yypvt[-2],
							  (struct node *) yypvt[-0]); } break;
case 127:
# line 359 "c0.y"
{ yyval = (int) nbina ('&', (struct node *) yypvt[-2],
							  (struct node *) yypvt[-0]); } break;
case 129:
# line 364 "c0.y"
{ yyval = (int) nbina (yypvt[-1], (struct node *) yypvt[-2],
							  (struct node *) yypvt[-0]); } break;
case 131:
# line 369 "c0.y"
{ yyval = (int) nbina (yypvt[-1], (struct node *) yypvt[-2],
							  (struct node *) yypvt[-0]); } break;
case 133:
# line 374 "c0.y"
{ yyval = (int) nbina (yypvt[-1], (struct node *) yypvt[-2],
							  (struct node *) yypvt[-0]); } break;
case 135:
# line 379 "c0.y"
{ yyval = (int) nbina (yypvt[-1], (struct node *) yypvt[-2],
							  (struct node *) yypvt[-0]); } break;
case 137:
# line 384 "c0.y"
{ yyval = (int) nbina (yypvt[-1], (struct node *) yypvt[-2],
							  (struct node *) yypvt[-0]); } break;
case 139:
# line 392 "c0.y"
{ yyval = (int) ncast ((struct type *) yypvt[-2],
							  (struct node *) yypvt[-0]); } break;
case 140:
# line 394 "c0.y"
{ yyval = (int) ncast ((struct type *) yypvt[-1],
							  (struct node *) 0); } break;
case 142:
# line 399 "c0.y"
{ yyval = (int) nun (yypvt[-1], (struct node *) yypvt[-0]); } break;
case 143:
# line 400 "c0.y"
{ yyval = (int) nun (yypvt[-1], (struct node *) yypvt[-0]); } break;
case 144:
# line 401 "c0.y"
{ yyval = (int) nun (yypvt[-1], (struct node *) yypvt[-0]); } break;
case 145:
# line 402 "c0.y"
{ yyval = (int) ninc (yypvt[-1],
							 (struct node *) yypvt[-0], 1); } break;
case 146:
# line 404 "c0.y"
{ yyval = (int) naddr ((struct node *) yypvt[-0]); } break;
case 147:
# line 405 "c0.y"
{ yyval = (int) nsize ((struct node *) 0,
							  (struct type *) yypvt[-1]); } break;
case 148:
# line 407 "c0.y"
{ yyval = (int) nsize ((struct node *) yypvt[-0],
						 (struct type *) 0); } break;
case 151:
# line 413 "c0.y"
{ yyval = (int) ncall ((struct node *) yypvt[-2],
							  (struct node *) 0); } break;
case 152:
# line 415 "c0.y"
{ yyval = (int) ncall ((struct node *) yypvt[-3],
							  (struct node *) yypvt[-1]); } break;
case 153:
# line 417 "c0.y"
{ yyval = (int) nindex ((struct node *) yypvt[-3],
							  (struct node *) yypvt[-1]); } break;
case 154:
# line 419 "c0.y"
{ yyval = (int) nmember ((struct node *) yypvt[-2], yypvt[-1],
							  yypvt[-0]); } break;
case 155:
# line 421 "c0.y"
{ yyval = (int) ninc (yypvt[-0], (struct node *) yypvt[-1], 0); } break;
case 157:
# line 425 "c0.y"
{ yyval = (int) ncomma ((struct node *) yypvt[-2],
							  (struct node *) yypvt[-0]); } break;
case 158:
# line 429 "c0.y"
{ yyval = (int) nname (yypvt[-0]); } break;
case 159:
# line 430 "c0.y"
{ yyval = (int) ncon (); } break;
case 160:
# line 431 "c0.y"
{ yyval = (int) nstr (yypvt[-0]); } break;
case 161:
# line 432 "c0.y"
{ yyval = (int) nstrcat ((struct node *) yypvt[-1],
							  yypvt[-0]); } break;
case 162:
# line 434 "c0.y"
{ yyval = yypvt[-1]; } break;
		}
		goto yystack;  /* stack new state and value */

	}

