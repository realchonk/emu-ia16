/*
 * c4 -- machine-dependent IR (CIR\002) -> ELF32 object.
 *
 * Reads the MDIR stream on stdin, writes an ELF32 relocatable object.
 * All PROGBITS output goes into .text; .bss is separate.  The x86 encoder
 * and ELF writer are shared with the bootstrap assembler, but c4 reads
 * concrete instructions instead of NASM text.
 */
#include <unistd.h>
#include <fcntl.h>
#include <stdio.h>

typedef unsigned char	byte;
typedef unsigned short	word;

#define LINEBUF		256
#define MAXGSYM		2048
#define GNAMEPOOL	20000
#define MAXLSYM		2048
#define LNAMEPOOL	1280
#define MAXFIX		2048
#define MAXNAME		64

#define S_UNDEF		0xff
#define F_GLOBAL	0x01
#define F_EXTERN	0x02
#define F_REF		0x04

#define OT_R8		1
#define OT_R16		2
#define OT_IMM		4
#define OT_MEM		5
#define OT_SEG		6

#define NOBASE		0xff
#define W(a, b) (((a)->type == OT_R8 || (b)->type == OT_R8) ? 1 : 2)

struct sec {
	word	size;
	word	pos;
};

struct sym {
	word	name;
	word	value;
	byte	sec;
	byte	flags;
};

struct op {
	byte	type;
	byte	reg;
	byte	size;
	byte	base;
	int	disp;
	int	sym;
};

static struct sec	secs[2];
static struct sym	gsyms[MAXGSYM];
static int		ngsyms;
static char		gnames[GNAMEPOOL];
static word		gnameused;
static struct sym	lsyms[MAXLSYM];
static int		nlsyms;
static char		lnames[LNAMEPOOL];
static word		lnameused;

static char	line[LINEBUF];
static char	*linep;
static char	rdbuf[128];
static int	rdpos, rdlen, ateof;
static int	infd, outfd, symfd;
static int	doemit;
static int	cursec;
static word	textoff;

static int		nrels;
static word		reloff;
static int		symelf ();

struct fix {
	word	off;
	byte	sym;
	byte	pcrel;
};
static struct fix	fixes[MAXFIX];
static int		nfix;

static int	esym;
static void	flushout ();
static struct op	ops[2];
static int		nops;

/* ---- helpers ---------------------------------------------------------- */

static int
slen (s)
const char *s;
{
	const char *p;

	p = s;
	while (*p)
		++p;
	return p - s;
}

static void
scopy (d, s)
char *d;
const char *s;
{
	while ((*d++ = *s++) != 0)
		;
}

static int
streq (a, b)
const char *a, *b;
{
	while (*a && *a == *b) {
		++a;
		++b;
	}
	return *a == *b;
}

static void
fail (msg)
const char *msg;
{
	write (2, "as: ", 4);
	write (2, msg, slen (msg));
	write (2, "\n", 1);
	_exit (1);
}

static void
ws ()
{
	while (*linep == ' ' || *linep == '\t')
		++linep;
}

static char idtab[128] = {
	0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
	0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
	0, 0, 0, 0, 1, 0, 0, 0, 0, 0, 0, 0, 0, 0, 1, 0,
	1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 0, 0, 0, 0, 0, 1,
	1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1,
	1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 0, 0, 0, 0, 1,
	0, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1,
	1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 0, 0, 0, 0, 0,
};

static int
isidc (c)
int c;
{
	return c >= 0 && c < 128 && idtab[c];
}

static int
isids (c)
int c;
{
	return c >= 0 && c < 128 && idtab[c] && (c < '0' || c > '9');
}

static void
readname (buf)
char *buf;
{
	int	n;

	n = 0;
	while (isidc (*linep)) {
		if (n < MAXNAME - 1)
			buf[n++] = *linep;
		++linep;
	}
	buf[n] = 0;
}

static char *
findL (s)
const char *s;
{
	while (*s) {
		if (s[0] == '.' && s[1] == 'L')
			return (char *) s;
		++s;
	}
	return 0;
}

static char *
stripdollar (s)
const char *s;
{
	if (s[0] == '$' && s[1] != 0)
		return (char *) s + 1;
	return (char *) s;
}

static int
fsym (tab, names, pn, pnused, max, pool, name, create)
struct sym *tab;
char *names;
int *pn, *pnused, max, pool;
const char *name;
int create;
{
	int	i, n;

	for (i = 0; i < *pn; ++i)
		if (streq (names + tab[i].name, name))
			return i;
	if (!create)
		return -1;
	if (*pn >= max)
		fail ("too many symbols");
	i = *pn;
	n = slen (name);
	if (*pnused + n + 1 > pool)
		fail ("name pool full");
	tab[i].name = *pnused;
	scopy (names + *pnused, name);
	*pnused += n + 1;
	tab[i].value = 0;
	tab[i].sec = S_UNDEF;
	tab[i].flags = F_REF;
	*pn = i + 1;
	return i;
}

static void
addfix (off, sym, pcrel)
int off, sym, pcrel;
{
	if (nfix >= MAXFIX)
		fail ("too many local fixups");
	fixes[nfix].off = off;
	fixes[nfix].sym = sym;
	fixes[nfix].pcrel = pcrel;
	++nfix;
}

static void
resetlocals ()
{
	byte	b[2];
	int	i, v;

	for (i = 0; i < nfix; ++i) {
		if (lsyms[fixes[i].sym].sec == S_UNDEF)
			fail ("undefined local");
		v = lsyms[fixes[i].sym].value;
		if (fixes[i].pcrel)
			v -= fixes[i].off + 2;
		flushout ();
		b[0] = v & 0xff;
		b[1] = (v >> 8) & 0xff;
		lseek (outfd, (long) (textoff + fixes[i].off), SEEK_SET);
		write (outfd, b, 2);
	}
	nfix = 0;
	nlsyms = 0;
	lnameused = 0;
}

static int
findsym (name, create)
const char *name;
int create;
{
	char	*n;

	n = stripdollar (name);
	return fsym (gsyms, gnames, &ngsyms, &gnameused,
		     MAXGSYM, GNAMEPOOL, n, create);
}

static int
symsec (s)
int s;
{
	return (s & 0x4000) ? lsyms[s & 0x3fff].sec : gsyms[s].sec;
}

static int
symval (s)
int s;
{
	return (s & 0x4000) ? lsyms[s & 0x3fff].value : gsyms[s].value;
}

static void
defsym (name, sec, value)
const char *name;
int sec, value;
{
	int	s;

	s = findsym (name, 1);
	if (s & 0x4000) {
		lsyms[s & 0x3fff].sec = sec;
		lsyms[s & 0x3fff].value = value;
		lsyms[s & 0x3fff].flags &= ~F_EXTERN;
	} else {
		gsyms[s].sec = sec;
		gsyms[s].value = value;
		gsyms[s].flags &= ~F_EXTERN;
	}
}

static void
marksym (name, flag)
const char *name;
int flag;
{
	int	s;

	s = fsym (gsyms, gnames, &ngsyms, &gnameused,
		  MAXGSYM, GNAMEPOOL, stripdollar (name), 1);
	gsyms[s].flags |= flag;
}

static void
switchsec (name)
const char *name;
{
	cursec = (name[1] == 'b') ? 1 : 0;
}

/* ---- output ----------------------------------------------------------- */

static byte	obuf[128];
static int	ocnt;
static word	ooff;

static void
flushout ()
{
	if (ocnt != 0) {
		lseek (outfd, (long) (textoff + ooff), SEEK_SET);
		write (outfd, obuf, ocnt);
		ooff += ocnt;
		ocnt = 0;
	}
}

static void
outb (b)
int b;
{
	if (!doemit) {
		++secs[0].pos;
		return;
	}
	if (cursec != 0)
		fail ("data in .bss");
	if (ocnt == 0)
		ooff = secs[0].pos;
	if (ocnt >= (int) sizeof obuf)
		flushout ();
	obuf[ocnt++] = b;
	++secs[0].pos;
}

static void
outw (w)
int w;
{
	outb (w & 0xff);
	outb ((w >> 8) & 0xff);
}

static void
advance (n)
int n;
{
	if (n <= 0)
		return;
	if (doemit && cursec == 0)
		flushout ();
	secs[cursec].pos += n;
}

static void
addrel (off, target, pcrel)
int off, target, pcrel;
{
	byte	b[8];
	int	esym, type;

	if (target & 0x8000)
		esym = 1 + (target & 1);
	else
		esym = symelf (target & 0x3fff);
	type = pcrel ? 21 : 20;
	b[0] = off & 0xff;
	b[1] = (off >> 8) & 0xff;
	b[2] = 0;
	b[3] = 0;
	b[4] = type;
	b[5] = esym & 0xff;
	b[6] = (esym >> 8) & 0xff;
	b[7] = 0;
	if (reloff + nrels * 8 < reloff)
		fail ("object too large");
	lseek (outfd, (long) (reloff + nrels * 8), SEEK_SET);
	write (outfd, b, 8);
	++nrels;
}

static void
emit_abs (sym, val, size)
int sym, val, size;
{
	int	off, sec;

	if (!doemit) {
		advance (size);
		return;
	}
	if (cursec != 0)
		fail ("data in .bss");
	off = secs[0].pos;
	if (sym < 0) {
		if (size == 1)
			outb (val);
		else
			outw (val);
		return;
	}
	sec = symsec (sym);
	if (sec == S_UNDEF) {
		if (sym & 0x4000) {
			if (size != 2 || val != 0)
				fail ("bad local reference");
			addfix (off, sym & 0x3fff, 0);
			outw (0);
			return;
		}
		addrel (off, sym & 0x3fff, 0);
	} else {
		addrel (off, 0x8000 | sec, 0);
		val += symval (sym);
	}
	if (size == 1)
		outb (val);
	else
		outw (val);
}

static void
emit_pc (sym, val)
int sym, val;
{
	int	off, sec;

	if (!doemit) {
		advance (2);
		return;
	}
	off = secs[0].pos;
	if (sym < 0) {
		outw (val - (off + 2));
		return;
	}
	sec = symsec (sym);
	if (sec == cursec && sec != S_UNDEF) {
		outw (symval (sym) + val - (off + 2));
		return;
	}
	if (sec == S_UNDEF) {
		if (sym & 0x4000) {
			if (val != 0)
				fail ("bad local reference");
			addfix (off, sym & 0x3fff, 1);
			outw (0);
			return;
		}
		addrel (off, sym & 0x3fff, 1);
		outw (val - 2);
	} else {
		addrel (off, 0x8000 | sec, 1);
		outw (symval (sym) + val - 2);
	}
}

/* ---- expressions ------------------------------------------------------ */

static int
parsenum ()
{
	int	v;

	if (*linep < '0' || *linep > '9')
		fail ("number expected");
	v = 0;
	while (*linep >= '0' && *linep <= '9') {
		v = v * 10 + (*linep - '0');
		++linep;
	}
	return v;
}

static int
expr ()
{
	int	v, s, sign;
	char	name[MAXNAME];

	ws ();
	s = -1;
	if (*linep >= '0' && *linep <= '9')
		v = parsenum ();
	else {
		readname (name);
		if (name[0] == 0)
			fail ("expression expected");
		s = findsym (name, 1);
		v = 0;
	}
	for (;;) {
		ws ();
		if (*linep == '+' || *linep == '-') {
			sign = (*linep == '-') ? -1 : 1;
			++linep;
			ws ();
			v += sign * parsenum ();
		} else
			break;
	}
	esym = s;
	return v;
}

/* ---- operands --------------------------------------------------------- */

static int
reg16 (name)
const char *name;
{
	static const char *n[] = {
		"ax", "cx", "dx", "bx", "sp", "bp", "si", "di"
	};
	int	i;

	for (i = 0; i < 8; ++i)
		if (streq (name, n[i]))
			return i;
	return -1;
}

static int
reg8 (name)
const char *name;
{
	static const char *n[] = {
		"al", "cl", "dl", "bl", "ah", "ch", "dh", "bh"
	};
	int	i;

	for (i = 0; i < 8; ++i)
		if (streq (name, n[i]))
			return i;
	return -1;
}

static void
parsemem (o)
struct op *o;
{
	char	name[MAXNAME];
	int	id, sign;

	++linep;
	o->type = OT_MEM;
	ws ();
	readname (name);
	id = reg16 (name);
	if (id == 3 || id == 5 || id == 6 || id == 7) {
		o->base = id;
		ws ();
		if (*linep == '+' || *linep == '-') {
			sign = (*linep == '-') ? -1 : 1;
			++linep;
			ws ();
			o->disp = sign * parsenum ();
		}
	} else
		fail ("bad address");
	ws ();
	if (*linep != ']')
		fail ("']' expected");
	++linep;
}

static void
parseoperand (o, osz)
struct op *o;
int osz;
{
	char	name[MAXNAME];
	char	*save;
	int	id;

	o->type = 0;
	o->size = osz;
	o->base = NOBASE;
	o->reg = 0;
	o->disp = 0;
	o->sym = -1;

	ws ();
	if (*linep == '[') {
		parsemem (o);
		return;
	}
	save = linep;
	readname (name);
	if (streq (name, "byte") || streq (name, "word")) {
		o->size = (name[0] == 'b') ? 1 : 2;
		ws ();
		if (*linep != '[')
			fail ("memory expected");
		parsemem (o);
		return;
	}
	id = reg16 (name);
	if (id >= 0) {
		o->type = OT_R16;
		o->reg = id;
		o->size = 2;
		return;
	}
	id = reg8 (name);
	if (id >= 0) {
		o->type = OT_R8;
		o->reg = id;
		o->size = 1;
		return;
	}
	linep = save;
	o->type = OT_IMM;
	o->disp = expr ();
	o->sym = esym;
}

static byte rmtab[8] = { 0, 0, 0, 7, 0, 6, 4, 5 };

static void
emit_modrm (reg, o)
int reg;
struct op *o;
{
	int	rm, mod;

	if (o->base == NOBASE) {
		/* absolute 16-bit displacement, optionally symbolic */
		outb (((reg & 7) << 3) | 6);
		emit_abs (o->sym, o->disp, 2);
		return;
	}
	rm = rmtab[o->base & 7];
	if (o->disp == 0 && rm != 6)
		mod = 0;
	else if (o->disp >= -128 && o->disp <= 127)
		mod = 1;
	else
		mod = 2;
	outb ((mod << 6) | ((reg & 7) << 3) | rm);
	if (mod == 1)
		outb (o->disp & 0xff);
	else if (mod == 2)
		outw (o->disp);
}

static void
emit_rm (reg, o)
int reg;
struct op *o;
{
	if (o->type == OT_MEM)
		emit_modrm (reg, o);
	else
		outb (0xc0 | ((reg & 7) << 3) | (o->reg & 7));
}

static void
emit_imm (o, size)
struct op *o;
int size;
{
	emit_abs (o->sym, o->disp, size);
}

/* ---- instruction encoding --------------------------------------------- */

static const char *aluname[] = {
	"add", "or", "adc", "sbb", "and", "sub", "xor", "cmp", 0
};

static const char *jnames[] = {
	"jb", "jae", "je", "jne", "jbe", "ja", "jz", "jnz", 0
};
static const byte jcode[] = { 2, 3, 4, 5, 6, 7, 4, 5 };

static int
ccindex (mn)
const char *mn;
{
	int	i;

	for (i = 0; jnames[i] != 0; ++i)
		if (streq (mn, jnames[i]))
			return jcode[i];
	return -1;
}

static void
enc_alu (idx)
int idx;
{
	struct op *a, *b;
	int	w;

	if (nops != 2)
		fail ("two operands expected");
	a = &ops[0];
	b = &ops[1];
	if (a->type != OT_R16 && a->type != OT_R8)
		fail ("bad operands");
	w = W (a, b);
	if (b->type == OT_IMM) {
		if (a->reg == 0) {
			outb (idx * 8 + (w == 1 ? 4 : 5));
			emit_imm (b, w);
		} else {
			outb (w == 1 ? 0x80 : 0x81);
			outb (0xc0 | (idx << 3) | a->reg);
			emit_imm (b, w);
		}
	} else {
		outb (idx * 8 + (w == 1 ? 2 : 3));
		emit_rm (a->reg, b);
	}
}

static void
enc_mov ()
{
	struct op *a, *b;
	int	w;

	if (nops != 2)
		fail ("two operands expected");
	a = &ops[0];
	b = &ops[1];
	if (a->type == OT_SEG) {
		if ((b->type != OT_R16 && b->type != OT_MEM))
			fail ("bad segment move");
		outb (0x8e);
		emit_rm (a->reg, b);
		return;
	}
	if (b->type == OT_SEG) {
		if (a->type != OT_R16 && a->type != OT_MEM)
			fail ("bad segment move");
		outb (0x8c);
		emit_rm (b->reg, a);
		return;
	}
	w = W (a, b);
	if (a->type == OT_R16 || a->type == OT_R8) {
		if (b->type == OT_IMM) {
			outb ((w == 1 ? 0xb0 : 0xb8) + a->reg);
			emit_imm (b, w);
		} else {
			outb (w == 1 ? 0x8a : 0x8b);
			emit_rm (a->reg, b);
		}
	} else if (a->type == OT_MEM) {
		if (b->type == OT_IMM) {
			outb (w == 1 ? 0xc6 : 0xc7);
			emit_modrm (0, a);
			emit_imm (b, w);
		} else if (b->type == OT_R8 || b->type == OT_R16) {
			outb (w == 1 ? 0x88 : 0x89);
			emit_rm (b->reg, a);
		} else
			fail ("bad operands");
	} else
		fail ("bad operands");
}

static void
enc_test ()
{
	struct op *a, *b;

	if (nops != 2)
		fail ("two operands expected");
	a = &ops[0];
	b = &ops[1];
	if ((a->type != OT_R16 && a->type != OT_R8) ||
	    (b->type != OT_R16 && b->type != OT_R8))
		fail ("bad operands");
	outb (W (a, b) == 1 ? 0x84 : 0x85);
	outb (0xc0 | (a->reg << 3) | b->reg);
}

static void
enc_unary (digit)
int digit;
{
	int	w;

	if (nops != 1)
		fail ("one operand expected");
	w = ops[0].size != 0 ? ops[0].size :
	    (ops[0].type == OT_R8 ? 1 : 2);
	outb (w == 1 ? 0xf6 : 0xf7);
	emit_rm (digit, &ops[0]);
}

static void
enc_shift (digit)
int digit;
{
	if (nops != 2 || ops[0].type != OT_R16 || ops[0].reg != 0 ||
	    ops[1].type != OT_R8 || ops[1].reg != 1)
		fail ("bad shift");
	outb (0xd3);
	outb (0xe0 | (digit << 3));
}

static void
enc_branch (iscall, relop)
int iscall, relop;
{
	struct op *a;

	if (nops != 1)
		fail ("one operand expected");
	a = &ops[0];
	if (a->type != OT_IMM) {
		if (a->type != OT_R16 && a->type != OT_MEM)
			fail ("bad branch");
		outb (0xff);
		emit_rm (iscall ? 2 : 4, a);
		return;
	}
	/* A backward branch to an already-defined local label can use the
	   short form.  Forward branches stay near; their fixups are patched
	   with the final label value, so later size changes are harmless. */
	if (!iscall && (a->sym & 0x4000)) {
		int sec = symsec (a->sym);
		if (sec == cursec && sec != S_UNDEF) {
			int off = secs[0].pos;
			int d = symval (a->sym) + a->disp - (off + 2);
			if (d >= -128 && d <= 127) {
				outb (relop >= 0 ? 0x70 + relop : 0xeb);
				outb (d & 0xff);
				return;
			}
		}
	}
	if (iscall) {
		outb (0xe8);
		emit_pc (a->sym, a->disp);
	} else if (relop >= 0) {
		outb (0x0f);
		outb (relop + 0x80);
		emit_pc (a->sym, a->disp);
	} else {
		outb (0xe9);
		emit_pc (a->sym, a->disp);
	}
}

static void
encode_insn (mn)
const char *mn;
{
	int	i;

	for (i = 0; aluname[i] != 0; ++i)
		if (streq (mn, aluname[i])) {
			enc_alu (i);
			return;
		}
	if (streq (mn, "mov")) { enc_mov (); return; }
	if (streq (mn, "lea")) {
		if (nops != 2 || ops[0].type != OT_R16 ||
		    ops[1].type != OT_MEM)
			fail ("bad lea");
		outb (0x8d);
		emit_modrm (ops[0].reg, &ops[1]);
		return;
	}
	if (streq (mn, "test")) { enc_test (); return; }
	if (streq (mn, "neg")) { enc_unary (3); return; }
	if (streq (mn, "not")) { enc_unary (2); return; }
	if (streq (mn, "mul")) { enc_unary (4); return; }
	if (streq (mn, "div")) { enc_unary (6); return; }
	if (streq (mn, "push")) {
		if (nops != 1)
			fail ("bad push");
		if (ops[0].type == OT_SEG) {
			outb (0x06 + (ops[0].reg << 3));
			return;
		}
		if (ops[0].type != OT_R16)
			fail ("bad push");
		outb (0x50 + ops[0].reg);
		return;
	}
	if (streq (mn, "pop")) {
		if (nops != 1)
			fail ("bad pop");
		if (ops[0].type == OT_SEG) {
			outb (0x07 + (ops[0].reg << 3));
			return;
		}
		if (ops[0].type != OT_R16)
			fail ("bad pop");
		outb (0x58 + ops[0].reg);
		return;
	}
	if (streq (mn, "shl")) { enc_shift (4); return; }
	if (streq (mn, "shr")) { enc_shift (5); return; }
	if (streq (mn, "call")) { enc_branch (1, -1); return; }
	if (streq (mn, "jmp")) { enc_branch (0, -1); return; }
	i = ccindex (mn);
	if (i >= 0) { enc_branch (0, i); return; }
	if (streq (mn, "ret")) { outb (0xc3); return; }
	if (streq (mn, "cwd")) { outb (0x99); return; }
	fail ("unknown instruction");
}

/* ---- directives and line parser --------------------------------------- */

static void
do_data (size)
int size;
{
	int	v;

	v = expr ();
	emit_abs (esym, v, size);
}

static void
do_res ()
{
	int	n;

	n = expr ();
	if (esym >= 0)
		fail ("constant expected");
	advance (n);
}

static void
do_statement ()
{
	char	mn[MAXNAME];
	char	name[MAXNAME];
	char	*save;
	int	osz;

	ws ();
	if (*linep == 0)
		return;
	if (*linep == '[') {
		while (*linep)
			++linep;
		return;
	}
	readname (mn);
	if (mn[0] == 's' && mn[1] == 'e' && mn[2] == 'c') {
		ws ();
		readname (name);
		switchsec (name);
		return;
	}
	if (mn[0] == 'g' && mn[1] == 'l') {
		ws ();
		readname (name);
		marksym (name, F_GLOBAL);
		return;
	}
	if (mn[0] == 'e' && mn[1] == 'x') {
		ws ();
		readname (name);
		marksym (name, F_EXTERN);
		return;
	}
	if (mn[0] == 'd' && mn[1] == 'b') { do_data (1); return; }
	if (mn[0] == 'd' && mn[1] == 'w') { do_data (2); return; }
	if (mn[0] == 'r' && mn[1] == 'e' && mn[2] == 's') {
		do_res ();
		return;
	}

	osz = 0;
	ws ();
	save = linep;
	readname (name);
	if (streq (name, "byte"))
		osz = 1;
	else if (streq (name, "word"))
		osz = 2;
	else
		linep = save;

	nops = 0;
	ws ();
	while (*linep != 0) {
		if (nops >= 2)
			fail ("too many operands");
		parseoperand (&ops[nops], nops == 0 ? osz : 0);
		++nops;
		ws ();
		if (*linep == ',') {
			++linep;
			continue;
		}
		if (*linep != 0)
			fail ("garbage after operands");
		break;
	}
	encode_insn (mn);
}

/* ---- input ------------------------------------------------------------ */

static int
nextline ()
{
	int	c, n;

	n = 0;
	ateof = 0;
	for (;;) {
		if (rdpos >= rdlen) {
			rdlen = read (infd, rdbuf, sizeof rdbuf);
			rdpos = 0;
			if (rdlen <= 0) {
				ateof = 1;
				break;
			}
		}
		c = rdbuf[rdpos++];
		if (c == '\n')
			break;
		if (c == '\r')
			continue;
		if (n < LINEBUF - 1)
			line[n++] = c;
	}
	line[n] = 0;
	while (n > 0 && (line[n - 1] == ' ' || line[n - 1] == '\t'))
		line[--n] = 0;
	linep = line;
	return n != 0 || !ateof;
}

static void
assemble (p)
int p;
{
	char	name[MAXNAME];
	char	*save;

	doemit = p == 2;
	rdpos = rdlen = 0;
	ateof = 0;
	cursec = 0;
	if (p == 2) {
		secs[0].pos = 0;
		secs[1].pos = 0;
		ocnt = 0;
		ooff = 0;
		nrels = 0;
		nlsyms = 0;
		lnameused = 0;
		nfix = 0;
	}
	lseek (infd, 0L, SEEK_SET);
	while (nextline ()) {
		ws ();
		if (*linep == 0)
			continue;
		if (isids (*linep)) {
			save = linep;
			readname (name);
			ws ();
			if (*linep == ':') {
				++linep;
				if (name[0] != '.' && findL (stripdollar (name)) == 0)
					resetlocals ();
				defsym (name, cursec, secs[cursec].pos);
				ws ();
				if (*linep == 0)
					continue;
			} else
				linep = save;
		}
		do_statement ();
	}
	if (p == 2) {
		resetlocals ();
		flushout ();
	} else {
		secs[0].size = secs[0].pos;
		secs[1].size = secs[1].pos;
	}
}

/* ---- ELF writer ------------------------------------------------------- */

static char shstr[48] = {
	0, 46, 116, 101, 120, 116, 0, 46, 98, 115, 115, 0, 46, 114, 101, 108,
	46, 116, 101, 120, 116, 0, 46, 115, 121, 109, 116, 97, 98, 0, 46, 115,
	116, 114, 116, 97, 98, 0, 46, 115, 104, 115, 116, 114, 116, 97, 98, 0,
};

static byte ehdr[52] = {
	0x7f, 'E', 'L', 'F', 1, 1, 1, 0, 0, 0, 0, 0, 0, 0, 0, 0,
	1, 0, 3, 0, 1, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
	0, 0, 0, 0, 0, 0, 0, 0, 52, 0, 0, 0, 0, 0, 40, 0,
	7, 0, 6, 0
};

static word	symoff, stroff, shstroff, shdroff;
static word	strsize;
static int	nelf;

static void	put16 ();
static void	writeat ();

static void
put_shdr (idx, name, type, flags, off, size, link, info, align, entsize)
int idx, name, type, flags, off, size, link, info, align, entsize;
{
	byte	b[40];
	int	i;

	for (i = 0; i < 40; ++i)
		b[i] = 0;
	put16 (b + 0, name);
	put16 (b + 4, type);
	put16 (b + 8, flags);
	put16 (b + 16, off);
	put16 (b + 20, size);
	put16 (b + 24, link);
	put16 (b + 28, info);
	put16 (b + 32, align);
	put16 (b + 36, entsize);
	writeat (shdroff + idx * 40, b, 40);
}

static void
write_shdrs ()
{
	put_shdr (0, 0, 0, 0, 0, 0, 0, 0, 0, 0);
	put_shdr (1, 1, 1, 6, textoff, secs[0].size, 0, 0, 16, 0);
	put_shdr (2, 7, 8, 3, textoff + secs[0].size, secs[1].size,
		  0, 0, 4, 0);
	put_shdr (3, 12, 9, 0, reloff, nrels * 8, 4, 1, 4, 8);
	put_shdr (4, 22, 2, 0, symoff, nelf * 16, 5, 3, 4, 16);
	put_shdr (5, 30, 3, 0, stroff, strsize, 0, 0, 1, 0);
	put_shdr (6, 38, 3, 0, shstroff, sizeof shstr, 0, 0, 1, 0);
}


static int
emit_sym (i)
int i;
{
	if (gsyms[i].sec == S_UNDEF)
		return (gsyms[i].flags & (F_GLOBAL | F_EXTERN | F_REF)) != 0;
	return (gsyms[i].flags & F_GLOBAL) != 0;
}

static int
symelf (i)
int i;
{
	int	j, n;

	n = 3;
	for (j = 0; j < i; ++j)
		if (emit_sym (j))
			++n;
	return n;
}

static void
put16 (p, v)
byte *p;
int v;
{
	p[0] = v & 0xff;
	p[1] = (v >> 8) & 0xff;
}

static void
writeat (off, buf, n)
unsigned off;
const byte *buf;
int n;
{
	lseek (outfd, (long) off, SEEK_SET);
	if (write (outfd, buf, n) != n)
		fail ("write failed");
}

static void
write_header ()
{
	ehdr[32] = shdroff & 0xff;
	ehdr[33] = (shdroff >> 8) & 0xff;
	writeat (0, ehdr, 52);
}

static void
write_symtab ()
{
	byte	b[16];
	int	i, n, stro;

	stro = 1;
	lseek (outfd, (long) symoff, SEEK_SET);
	for (n = 0; n < 16; ++n)
		b[n] = 0;
	write (outfd, b, 16);

	b[12] = 3;
	for (i = 0; i < 2; ++i) {
		put16 (b + 14, i + 1);
		write (outfd, b, 16);
	}

	for (n = 0; n < 12; ++n)
		b[n] = 0;
	b[12] = 0x10;
	b[13] = 0;
	for (i = 0; i < ngsyms; ++i) {
		if (!emit_sym (i))
			continue;
		put16 (b + 0, stro);
		stro += slen (gnames + gsyms[i].name) + 1;
		if (gsyms[i].sec != S_UNDEF) {
			put16 (b + 4, gsyms[i].value);
			put16 (b + 14, gsyms[i].sec + 1);
		} else {
			put16 (b + 4, 0);
			put16 (b + 14, 0);
		}
		write (outfd, b, 16);
	}
}

static void
write_strtab ()
{
	byte	z;
	int	i;

	lseek (outfd, (long) stroff, SEEK_SET);
	z = 0;
	write (outfd, &z, 1);
	for (i = 0; i < ngsyms; ++i) {
		if (!emit_sym (i))
			continue;
		write (outfd, gnames + gsyms[i].name,
		       slen (gnames + gsyms[i].name));
		write (outfd, &z, 1);
	}
}

static void
addck (o, n)
unsigned o, n;
{
	if (o + n < o)
		fail ("object too large");
}

static void
elf_finish ()
{
	unsigned o;
	int	i;

	nelf = 3;
	strsize = 1;
	for (i = 0; i < ngsyms; ++i)
		if (emit_sym (i)) {
			++nelf;
			strsize += slen (gnames + gsyms[i].name) + 1;
		}
	o = reloff;
	if (o < textoff)
		fail ("object too large");
	addck (o, nrels * 8);
	o += nrels * 8;
	o = (o + 3) & ~3;
	symoff = o;
	addck (o, nelf * 16);
	o += nelf * 16;
	stroff = o;
	addck (o, strsize);
	o += strsize;
	shstroff = o;
	addck (o, sizeof shstr);
	o += sizeof shstr;
	o = (o + 3) & ~3;
	shdroff = o;
	addck (o, 280);

	write_header ();
	write_symtab ();
	write_strtab ();
	writeat (shstroff, (byte *) shstr, sizeof shstr);
	write_shdrs ();
}


/* ---- MDIR reader ------------------------------------------------------ */

#define SC_EXTERN	0
#define SC_GLOBAL	1
#define SC_STATIC	2
#define SZ_B		0
#define SZ_W		1
#define SZ_D		2

static byte
mgetb ()
{
	byte b;
	if (read (infd, &b, 1) != 1)
		fail ("mdi: unexpected eof");
	return b;
}

static word
mgetw ()
{
	word w;
	if (read (infd, &w, 2) != 2)
		fail ("mdi: unexpected eof");
	return w;
}

static char *
mdname (sym, buf)
word sym;
char *buf;
{
	int	n;

	n = pread (symfd, buf, MAXNAME - 1, (long) sym);
	if (n < 0)
		n = 0;
	buf[n] = 0;
	return buf;
}

/* Generated labels are private to one function and need no names. */
static int
findlbl (id, create)
word id;
int create;
{
	int	i;

	for (i = 0; i < nlsyms; ++i)
		if (lsyms[i].name == id)
			return 0x4000 | i;
	if (!create)
		fail ("undefined label");
	if (nlsyms >= MAXLSYM)
		fail ("too many labels");
	lsyms[nlsyms].name = id;
	lsyms[nlsyms].value = 0;
	lsyms[nlsyms].sec = S_UNDEF;
	lsyms[nlsyms].flags = F_REF;
	return 0x4000 | nlsyms++;
}

static int
mdsym (s, buf)
word s;
char *buf;
{
	if (s & 0x8000)
		return findlbl (s & 0x7fff, 1);
	return findsym (mdname (s, buf), 1);
}

static void
mdlabel (s, buf)
word s;
char *buf;
{
	int	i;

	if (s & 0x8000) {
		i = findlbl (s & 0x7fff, 1) & 0x3fff;
		lsyms[i].sec = 0;
		lsyms[i].value = secs[0].pos;
		lsyms[i].flags &= ~F_EXTERN;
	} else
		defsym (mdname (s, buf), 0, secs[0].pos);
}

static const byte	wregmap[8] = { 0, 3, 1, 2, 6, 7, 4, 5 };
static const byte	bregmap[8] = { 0, 3, 1, 2, 0, 0, 0, 0 };

static int
mdop (o, size)
struct op *o;
int size;
{
	char	kind;
	char	buf[MAXNAME];
	word	s, a;

	o->type = 0;
	o->reg = 0;
	o->size = size == SZ_B ? 1 : 2;
	o->base = NOBASE;
	o->disp = 0;
	o->sym = -1;
	kind = mgetb ();
	switch (kind) {
	case 'R':
		o->reg = size == SZ_B ? bregmap[mgetb () & 7]
				      : wregmap[mgetb () & 7];
		o->type = size == SZ_B ? OT_R8 : OT_R16;
		break;
	case 'G':
		o->reg = mgetb ();
		o->type = OT_SEG;
		break;
	case 'B':
		o->type = OT_MEM;
		o->base = 5;		/* bp */
		o->disp = (short) mgetw ();
		break;
	case 'M':
		o->type = OT_MEM;
		o->base = wregmap[mgetb () & 7];
		o->disp = (short) mgetw ();
		break;
	case 'I':
		o->type = OT_IMM;
		o->disp = mgetw ();
		break;
	case 'T':
		o->type = OT_IMM;
		o->sym = findsym ("strtab", 1);
		o->disp = mgetw ();
		break;
	case 'S':
		s = mgetw ();
		a = mgetw ();
		o->type = OT_IMM;
		o->sym = mdsym (s, buf);
		o->disp = a;
		break;
	case 'A':
		s = mgetw ();
		a = mgetw ();
		o->type = OT_MEM;
		o->base = NOBASE;
		o->sym = mdsym (s, buf);
		o->disp = a;
		break;
	default:
		fail ("mdi: bad operand");
	}
	return 0;
}

static int
aluidx (op)
int op;
{
	switch (op) {
	case 'a': return 0;	/* add */
	case 'o': return 1;	/* or */
	case 'c': return 2;	/* adc */
	case 'b': return 3;	/* sbb */
	case 'n': return 4;	/* and */
	case 's': return 5;	/* sub */
	case 'x': return 6;	/* xor */
	case 'm': return 7;	/* cmp */
	}
	fail ("mdi: bad alu op");
	return 0;
}

static struct op	pprev[2], pcur[2];
static int		phave;

static int
same_mem (a, b)
struct op *a, *b;
{
	return a->type == OT_MEM && b->type == OT_MEM &&
	       a->base == b->base && a->disp == b->disp &&
	       a->sym == b->sym && a->size == b->size;
}

static void
flush_prev ()
{
	if (!phave)
		return;
	ops[0] = pprev[0];
	ops[1] = pprev[1];
	nops = 2;
	enc_mov ();
	phave = 0;
}

static void
md_mov ()
{
	int size;
	struct op v;

	size = mgetb ();
	mdop (&pcur[0], size);
	mdop (&pcur[1], size);
	if (!phave) {
		pprev[0] = pcur[0];
		pprev[1] = pcur[1];
		phave = 1;
		return;
	}

	/* store followed by a load from the same slot: use the stored
	   register/immediate directly instead of reloading memory. */
	if (pprev[0].type == OT_MEM &&
	    (pprev[1].type == OT_R16 || pprev[1].type == OT_R8 ||
	     pprev[1].type == OT_IMM) &&
	    (pcur[0].type == OT_R16 || pcur[0].type == OT_R8) &&
	    pcur[1].type == OT_MEM && same_mem (&pprev[0], &pcur[1])) {
		v = pprev[1];
		flush_prev ();
		if ((pcur[0].type == OT_R16 || pcur[0].type == OT_R8) &&
		    pcur[0].reg == v.reg && pcur[0].type == v.type) {
			phave = 0;
			return;
		}
		pcur[1] = v;
		pprev[0] = pcur[0];
		pprev[1] = pcur[1];
		phave = 1;
		return;
	}

	/* load followed by a store of the same register back to the same
	   slot: the store is redundant. */
	if ((pprev[0].type == OT_R16 || pprev[0].type == OT_R8) &&
	    pprev[1].type == OT_MEM && pcur[0].type == OT_MEM &&
	    (pcur[1].type == OT_R16 || pcur[1].type == OT_R8) &&
	    same_mem (&pprev[1], &pcur[0]) &&
	    pprev[0].type == pcur[1].type &&
	    pprev[0].reg == pcur[1].reg) {
		flush_prev ();
		phave = 0;
		return;
	}

	/* two stores to the same slot: the first is dead. */
	if (pprev[0].type == OT_MEM &&
	    (pprev[1].type == OT_R16 || pprev[1].type == OT_R8 ||
	     pprev[1].type == OT_IMM) &&
	    pcur[0].type == OT_MEM &&
	    (pcur[1].type == OT_R16 || pcur[1].type == OT_R8 ||
	     pcur[1].type == OT_IMM) &&
	    same_mem (&pprev[0], &pcur[0])) {
		pprev[0] = pcur[0];
		pprev[1] = pcur[1];
		return;
	}

	flush_prev ();
	pprev[0] = pcur[0];
	pprev[1] = pcur[1];
	phave = 1;
}

static void
mdinsn (tag)
int tag;
{
	int	size, op, i;

	if (tag != 'm')
		flush_prev ();
	switch (tag) {
	case 'm':
		md_mov ();
		break;
	case 'a':
		op = mgetb ();
		size = mgetb ();
		mdop (&ops[0], size);
		mdop (&ops[1], size);
		nops = 2;
		enc_alu (aluidx (op));
		break;
	case 't':
		size = mgetb ();
		mdop (&ops[0], size);
		mdop (&ops[1], size);
		nops = 2;
		enc_test ();
		break;
	case 'u':
		op = mgetb ();
		size = mgetb ();
		mdop (&ops[0], size);
		nops = 1;
		i = op == '-' ? 3 : op == '~' ? 2 : op == '*' ? 4 : 6;
		enc_unary (i);
		break;
	case 'h':
		op = mgetb ();
		size = mgetb ();
		mdop (&ops[0], size);
		mdop (&ops[1], SZ_B);
		nops = 2;
		enc_shift (op == 'l' ? 4 : 5);
		break;
	case 'l':
		mdop (&ops[0], SZ_W);
		mdop (&ops[1], SZ_W);
		nops = 2;
		encode_insn ("lea");
		break;
	case 'd':
		size = mgetb ();
		mdop (&ops[0], size);
		mdop (&ops[1], size);
		nops = 2;
		enc_mov ();
		break;
	case 's':
		size = mgetb ();
		mdop (&ops[0], size);
		mdop (&ops[1], size);
		nops = 2;
		enc_mov ();
		break;
	case 'p':
		mdop (&ops[0], SZ_W);
		nops = 1;
		encode_insn ("push");
		break;
	case 'o':
		mdop (&ops[0], SZ_W);
		nops = 1;
		encode_insn ("pop");
		break;
	case 'c':
		mdop (&ops[0], SZ_W);
		nops = 1;
		enc_branch (1, -1);
		break;
	case 'j':
		mdop (&ops[0], SZ_W);
		nops = 1;
		enc_branch (0, -1);
		break;
	case 'J':
		op = mgetb ();
		mdop (&ops[0], SZ_W);
		nops = 1;
		enc_branch (0, op);
		break;
	case 'r':
		outb (0xc3);
		break;
	case 'x':
		outb (mgetb ());
		break;
	default:
		fail ("mdi: bad instruction");
	}
}

static void
mddata ()
{
	char	buf[MAXNAME], sbuf[MAXNAME];
	byte	flags, c;
	word	sym, size, n, s, a, i;

	flags = mgetb ();
	sym = mgetw ();
	size = mgetw ();
	mdname (sym, buf);
	if ((flags & 3) == SC_EXTERN)
		return;		/* relocations add it if it is referenced */
	if ((flags & 3) == SC_GLOBAL)
		marksym (buf, F_GLOBAL);
	cursec = (flags & 4) ? 0 : 1;
	defsym (buf, cursec, secs[cursec].pos);
	if (!(flags & 4)) {
		advance (size);
		return;
	}
	for (;;) {
		c = mgetb ();
		if (c == 0)
			return;
		switch (c) {
		case 1:
			n = mgetw ();
			for (i = 0; i < n; ++i)
				outb (mgetb ());
			break;
		case 2:
			s = mgetw ();
			a = mgetw ();
			mdname (s, sbuf);
			emit_abs (findsym (sbuf, 1), a, 2);
			break;
		case 3:
			s = mgetw ();
			a = mgetw ();
			emit_abs (findsym ("strtab", 1), s + a, 2);
			break;
		default:
			fail ("mdi: bad data chunk");
		}
	}
}

static void
mdfunc ()
{
	char	buf[MAXNAME];
	byte	flags, tag, ns;
	word	sym, w, n, i, s;

	flags = mgetb ();
	sym = mgetw ();
	if (!(flags & 4)) {
		if (mgetb () != 0xff)
			fail ("mdi: bad declaration");
		return;		/* relocations add it if it is referenced */
	}
	while ((w = mgetw ()) != 0xffff)
		;
	while ((w = mgetw ()) != 0xffff)
		;
	mdname (sym, buf);
	if ((flags & 3) == SC_GLOBAL)
		marksym (buf, F_GLOBAL);
	defsym (buf, 0, secs[0].pos);
	cursec = 0;
	for (;;) {
		tag = mgetb ();
		if (tag == 0xff)
			break;
		if (tag == 'L') {
			flush_prev ();
			s = mgetw ();
			mdlabel (s, buf);
		} else
			mdinsn (tag);
	}
	flush_prev ();
	/* frame table */
	(void) mgetw ();
	ns = mgetb ();
	for (i = 0; i < ns; ++i)
		(void) mgetb ();
	n = mgetw ();
	for (i = 0; i < n; ++i)
		(void) mgetw ();
	if (mgetw () != 0xffff || mgetb () != 0xff)
		fail ("mdi: bad frame");
	resetlocals ();
}

static void
mdstr ()
{
	char	buf[MAXNAME];
	word	n, i;

	n = mgetw ();
	cursec = 0;
	scopy (buf, "strtab");
	defsym (buf, 0, secs[0].pos);
	for (i = 0; i < n; ++i)
		outb (mgetb ());
}

static void
mdparse ()
{
	byte	type;
	char	magic[4], nbuf[MAXNAME];

	if (read (infd, magic, 4) != 4 || magic[0] != 'C' ||
	    magic[1] != 'I' || magic[2] != 'R' || magic[3] != 2)
		fail ("mdi: bad magic");
	while (read (infd, &type, 1) == 1) {
		switch (type) {
		case 'D': mddata (); break;
		case 'F': mdfunc (); break;
		case 'T': mdstr (); break;
		case 'X': marksym (mdname (mgetw (), nbuf), F_EXTERN); break;
		default:
			fail ("mdi: bad record");
		}
	}
}

/* ---- main ------------------------------------------------------------- */

static void
usage ()
{
	write (2, "usage: c4 [-o out] symfile\n", 27);
	_exit (1);
}

int
main (argc, argv)
int argc;
char **argv;
{
	char	*symfile, *outname;
	int	i;

	symfile = 0;
	outname = 0;
	for (i = 1; i < argc; ++i) {
		if (streq (argv[i], "-o")) {
			if (++i >= argc)
				usage ();
			outname = argv[i];
		} else if (argv[i][0] != '-') {
			if (symfile != 0)
				usage ();
			symfile = argv[i];
		}
	}
	if (symfile == 0)
		usage ();
	if (outname == 0)
		outname = "a.o";

	symfd = open (symfile, O_RDONLY);
	if (symfd < 0)
		fail ("cannot open symbol file");
	outfd = open (outname, O_WRONLY | O_CREAT | O_TRUNC, 0644);
	if (outfd < 0)
		fail ("cannot open output");

	infd = 0;
	textoff = 64;

	doemit = 0;
	if (lseek (infd, 0L, SEEK_SET) != 0)
		fail ("cannot seek input");
	mdparse ();
	secs[0].size = secs[0].pos;
	secs[1].size = secs[1].pos;

	reloff = (textoff + secs[0].size + 3) & ~3;
	doemit = 1;
	secs[0].pos = 0;
	secs[1].pos = 0;
	ocnt = 0;
	ooff = 0;
	nrels = 0;
	if (lseek (infd, 0L, SEEK_SET) != 0)
		fail ("cannot seek input");
	mdparse ();
	flushout ();
	elf_finish ();
	close (symfd);
	close (outfd);
	return 0;
}
