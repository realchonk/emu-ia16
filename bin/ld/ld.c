/*
 * ld -- small two-pass linker for IA-16 flat binaries.
 *
 * Reads ELF32 relocatable objects and GNU ar archives, lays out .text,
 * .rodata, .data and .bss like lib/user.x, applies R_386_16 and R_386_PC16,
 * and writes a headerless flat image.  No malloc; all tables are static.
 */
#include <unistd.h>
#include <fcntl.h>
#include <err.h>
#include <string.h>
#include <stdlib.h>

typedef unsigned char	byte;
typedef unsigned short	word;
typedef unsigned long	dword;

#define MAXOBJ		160
#define MAXSEC		640
#define MAXSYM		900
#define NAMEPOOL	12000
#define MAXAR		8
#define MAXSEL		192
#define MAXDEF		32
#define MAXPATH		128
#define IOBUF		512

#define SHT_PROGBITS	1
#define SHT_SYMTAB	2
#define SHT_STRTAB	3
#define SHT_RELA	4
#define SHT_NOBITS	8
#define SHT_REL		9

#define SHN_UNDEF	0
#define SHN_ABS		0xfff1
#define SHN_COMMON	0xfff2

#define OUT_TEXT	0
#define OUT_RODATA	1
#define OUT_DATA	2
#define OUT_BSS		3
#define OUT_NONE	0xff

struct objinfo {
	word	path;		/* name pool offset */
	dword	base;		/* ELF start within file */
	dword	size;
	dword	shoff;
	word	shnum;
	word	shstrndx;
	word	shentsize;
	word	secbase;
};

struct secinfo {
	dword	off;
	dword	size;
	word	obj;
	word	nameoff;
	word	type;
	word	link;
	word	info;
	word	align;
	word	outsec;
	word	outaddr;
};

struct gsym {
	word	name;
	word	size;
	word	value;
	word	addr;
	word	obj;
	word	sec;
	word	align;
	byte	def;			/* 0 undef, 1 defined, 2 common, 3 abs */
	byte	weak;
};

static struct objinfo	objs[MAXOBJ];
static int		nobjs;
static struct secinfo	secs[MAXSEC];
static int		nsecs;
static struct gsym	syms[MAXSYM];
static int		nsyms;
static char		names[NAMEPOOL];
static int		nameused;
static dword		dots[4];
static dword		stacksize = 2048;
static char		*outpath = "a.out";
static char		outbuf[IOBUF];
static char		inbuf[IOBUF];
static char		namebuf[MAXPATH];
static char		pathbuf[MAXPATH];
static char		*ldirs[8];
static int		ndirs;
static char		arstore[MAXAR][MAXPATH];
static byte		arlib[MAXAR];

static struct {
	char	*name;
	word	value;
} defsyms[MAXDEF];
static int	ndefsyms;

static void
fail (msg)
const char *msg;
{
	errx (1, "ld: %s", msg);
}

static void
rderr (path)
const char *path;
{
	err (1, "ld: %s", path);
}

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

static int
scopy (d, s)
char *d;
const char *s;
{
	char *p;

	p = d;
	while ((*p++ = *s++) != 0)
		;
	return p - d - 1;
}

static int
nameadd (s)
const char *s;
{
	int n, o;

	n = slen (s) + 1;
	if (nameused + n > NAMEPOOL)
		fail ("name pool full");
	o = nameused;
	scopy (names + o, s);
	nameused += n;
	return o;
}

static char *
nameget (o)
int o;
{
	return names + o;
}

static word
rd16 (p)
const byte *p;
{
	return p[0] | (word) p[1] << 8;
}

static dword
rd32 (p)
const byte *p;
{
	return (dword) p[0] | (dword) p[1] << 8
	     | (dword) p[2] << 16 | (dword) p[3] << 24;
}

static void
wr16 (p, v)
byte *p;
word v;
{
	p[0] = v & 0xff;
	p[1] = (v >> 8) & 0xff;
}

static void
rd (fd, off, buf, n)
int fd;
dword off;
void *buf;
int n;
{
	int r;

	r = pread (fd, buf, n, (long) off);
	if (r != n) {
		if (r < 0)
			err (1, "ld: read");
		errx (1, "ld: unexpected end of file (off=%ld n=%d got=%d)",
		      (long) off, n, r);
	}
}

static void
wr (fd, off, buf, n)
int fd;
dword off;
const void *buf;
int n;
{
	int r;

	r = pwrite (fd, buf, n, (long) off);
	if (r != n) {
		if (r < 0)
			err (1, "ld: write");
		fail ("short write");
	}
}

/* ---- ELF --------------------------------------------------------------- */

static void
elf_check (fd, base)
int fd;
dword base;
{
	byte h[52];

	rd (fd, base, h, 52);
	if (h[0] != 0x7f || h[1] != 'E' || h[2] != 'L' || h[3] != 'F')
		fail ("not an ELF file");
	if (h[4] != 1 || h[5] != 1)
		fail ("not a little-endian ELF32 file");
	if (rd16 (h + 16) != 1 || rd16 (h + 18) != 3)
		fail ("not an i386 relocatable object");
}

static void
shdr (fd, at, sh)
int fd;
dword at;
struct secinfo *sh;
{
	byte h[40];

	rd (fd, at, h, 40);
	sh->off = rd32 (h + 16);
	sh->size = rd32 (h + 20);
	sh->nameoff = rd16 (h);
	sh->type = rd16 (h + 4);
	sh->link = rd16 (h + 24);
	sh->info = rd16 (h + 28);
	sh->align = rd16 (h + 32);
	sh->obj = 0;
	sh->outsec = OUT_NONE;
	sh->outaddr = 0;
}

/* section-name matching for the layout in lib/user.x */
static int
outsec_for (fd, base, nameoff, shstr)
int fd;
dword base;
word nameoff;
struct secinfo *shstr;
{
	int n;
	char c;

	namebuf[0] = 0;
	if (nameoff >= shstr->size)
		return OUT_NONE;
	for (n = 0; n < MAXPATH - 1; ++n) {
		rd (fd, base + shstr->off + nameoff + n, &c, 1);
		if (c == 0)
			break;
		namebuf[n] = c;
	}
	namebuf[n] = 0;
	if (strncmp (namebuf, ".text", 5) == 0)
		return OUT_TEXT;
	if (strncmp (namebuf, ".rodata", 7) == 0)
		return OUT_RODATA;
	if (strncmp (namebuf, ".data", 5) == 0)
		return OUT_DATA;
	if (strncmp (namebuf, ".bss", 4) == 0 ||
	    strncmp (namebuf, ".COMMON", 7) == 0)
		return OUT_BSS;
	return OUT_NONE;
}

static struct gsym *
symfind (name)
const char *name;
{
	int i;

	for (i = 1; i < nsyms; ++i)
		if (strcmp (nameget (syms[i].name), name) == 0)
			return &syms[i];
	return (struct gsym *) 0;
}

static struct gsym *
symget (name, create)
const char *name;
int create;
{
	struct gsym *s;

	s = symfind (name);
	if (s != (struct gsym *) 0 || !create)
		return s;
	if (nsyms >= MAXSYM)
		fail ("too many symbols");
	s = &syms[nsyms++];
	s->name = nameadd (name);
	s->size = 0;
	s->value = 0;
	s->addr = 0;
	s->obj = 0;
	s->sec = 0;
	s->align = 1;
	s->def = 0;
	s->weak = 0;
	return s;
}

static void
add_undef (name)
const char *name;
{
	struct gsym *s;

	s = symget (name, 1);
#ifdef DEBUG
	write (2, "U ", 2);
	write (2, name, slen (name));
	write (2, "\n", 1);
#endif
	if (s->def == 0)
		s->weak = s->weak && 0;
}

static void
add_def (name, obj, sec, value, weak)
const char *name;
int obj, sec;
word value;
int weak;
{
	struct gsym *s;

	s = symget (name, 1);
#ifdef DEBUG
	write (2, "F ", 2);
	write (2, name, slen (name));
	write (2, "\n", 1);
#endif
	if (s->def == 1) {
		if (!s->weak && !weak)
			errx (1, "ld: duplicate definition of '%s' (obj %d sec %d val %u)", name, obj, sec, value);
		if (!s->weak)
			return;
	}
	if (s->def == 0 || s->def == 2 || !s->weak) {
		s->def = 1;
		s->obj = obj;
		s->sec = sec;
		s->value = value;
		s->weak = weak;
	}
}

static void
add_common (name, size, align, weak)
const char *name;
word size;
word align;
int weak;
{
	struct gsym *s;

	s = symget (name, 1);
	if (s->def == 1)
		return;
	s->def = 2;
	s->size = size;
	s->align = align ? align : 1;
	s->value = 0;
	s->weak = weak;
}

static void
add_abs (name, value)
const char *name;
word value;
{
	struct gsym *s;

	s = symget (name, 1);
	s->def = 3;
	s->value = value;
	s->addr = value;
}

static int
symglobal (info)
byte info;
{
	int b;

	b = info >> 4;
	return b == 1 || b == 2;
}

static int
symweak (info)
byte info;
{
	return (info >> 4) == 2;
}

static void
symname (fd, base, strsec, off, buf)
int fd;
dword base;
struct secinfo *strsec;
word off;
char *buf;
{
	int n;
	char c;

	for (n = 0; n < MAXPATH - 1; ++n) {
		rd (fd, base + strsec->off + off + n, &c, 1);
		if (c == 0)
			break;
		buf[n] = c;
	}
	buf[n] = 0;
}

/* look up the symbol table and its string table for an object */
static int
find_symtab (obj, symsec, strsec)
int obj;
int *symsec;
int *strsec;
{
	int i, b;
	struct secinfo *s;

	b = objs[obj].secbase;
	for (i = 0; i < objs[obj].shnum; ++i) {
		s = &secs[b + i];
		if (s->type == SHT_SYMTAB) {
			*symsec = i;
			*strsec = s->link;
			return 0;
		}
	}
	return -1;
}

static void
register_object (obj, fd)
int obj;
int fd;
{
	byte e[16];
	struct secinfo *symsec, *strsec, *sh;
	int i, k, str;
	dword nsym, j;
	word ssz;
	byte info;

	if (find_symtab (obj, &i, &str) != 0)
		return;
	symsec = &secs[objs[obj].secbase + i];
	strsec = &secs[objs[obj].secbase + str];

	nsym = symsec->size / 16;
	for (j = 1; j < nsym; ++j) {
		rd (fd, objs[obj].base + symsec->off + j * 16, e, 16);
		info = e[12];
		if (!symglobal (info))
			continue;
		ssz = rd16 (e + 14);
		if (ssz == SHN_UNDEF || rd16 (e) == 0)
			continue;
		symname (fd, objs[obj].base, strsec, rd16 (e), namebuf);
		if (namebuf[0] == 0)
			continue;
		if (ssz == SHN_COMMON)
			add_common (namebuf, rd16 (e + 8), rd32 (e + 4),
				    symweak (info));
		else if (ssz == SHN_ABS)
			add_def (namebuf, obj, ssz, rd32 (e + 4),
				 symweak (info));
		else if (ssz < 0xff00)
			add_def (namebuf, obj, ssz, rd32 (e + 4),
				 symweak (info));
	}

	/* undefined references are defined by relocations, not by symtab */
	for (i = 0; i < objs[obj].shnum; ++i) {
		sh = &secs[objs[obj].secbase + i];
		if (sh->type != SHT_REL)
			continue;
		for (j = 0; j < sh->size / 8; ++j) {
			rd (fd, objs[obj].base + sh->off + j * 8 + 4, e, 4);
			k = rd32 (e) >> 8;
			rd (fd, objs[obj].base
			    + secs[objs[obj].secbase + sh->link].off
			    + k * 16, e, 16);
			if (rd16 (e + 14) != SHN_UNDEF ||
			    !symglobal (e[12]))
				continue;
			symname (fd, objs[obj].base,
				 &secs[objs[obj].secbase
				       + secs[objs[obj].secbase + sh->link].link],
				 rd16 (e), namebuf);
			if (namebuf[0] != 0)
				add_undef (namebuf);
		}
	}
}

static int
add_object (path, base, size, fd)
const char *path;
dword base, size;
int fd;
{
	struct objinfo *o;
	struct secinfo *shstr;
	int i, b;
	byte h[52];

	if (nobjs >= MAXOBJ)
		fail ("too many input objects");
	elf_check (fd, base);
	rd (fd, base, h, 52);
	o = &objs[nobjs];
	o->path = nameadd (path);
	o->base = base;
	o->size = size;
	o->shoff = rd32 (h + 32);
	o->shnum = rd16 (h + 48);
	o->shstrndx = rd16 (h + 50);
	o->shentsize = rd16 (h + 46);
	o->secbase = nsecs;
	if (o->shentsize < 40)
		fail ("bad section header size");
	if (nsecs + o->shnum > MAXSEC)
		fail ("too many sections");
	for (i = 0; i < o->shnum; ++i) {
		b = o->secbase + i;
		shdr (fd, base + o->shoff + (dword) i * o->shentsize,
		      &secs[b]);
		secs[b].obj = nobjs;
	}
	if (o->shstrndx >= o->shnum)
		fail ("bad section name table");
	shstr = &secs[o->secbase + o->shstrndx];
	for (i = 1; i < o->shnum; ++i) {
		b = o->secbase + i;
		if (secs[b].type == SHT_PROGBITS || secs[b].type == SHT_NOBITS)
			secs[b].outsec = outsec_for (fd, base, secs[b].nameoff,
						     shstr);
	}
	nsecs += o->shnum;
	nobjs++;
	register_object (nobjs - 1, fd);
	return 0;
}

static int
defined_here (fd, base, shnum, shoff, shentsize)
int fd;
dword base, shoff;
int shnum, shentsize;
{
	struct secinfo sym, str;
	byte h[40], e[16], ch;
	int i, j, str_index;
	dword nsym;
	struct gsym *s;

	str_index = -1;
	for (i = 0; i < shnum; ++i) {
		rd (fd, base + shoff + (dword) i * shentsize, h, 40);
		if (rd16 (h + 4) == SHT_SYMTAB) {
			sym.off = rd32 (h + 16);
			sym.size = rd32 (h + 20);
			sym.link = rd16 (h + 24);
			str_index = sym.link;
			break;
		}
	}
	if (str_index < 0)
		return 0;
	rd (fd, base + shoff + (dword) str_index * shentsize, h, 40);
	str.off = rd32 (h + 16);
	str.size = rd32 (h + 20);
	nsym = sym.size / 16;
	for (j = 1; j < nsym; ++j) {
		rd (fd, base + sym.off + j * 16, e, 16);
		if (!symglobal (e[12]) || rd16 (e + 14) == SHN_UNDEF)
			continue;
		for (i = 0; i < MAXPATH - 1; ++i) {
			rd (fd, base + str.off + rd16 (e) + i, &ch, 1);
			if (ch == 0)
				break;
			namebuf[i] = ch;
		}
		namebuf[i] = 0;
		s = symfind (namebuf);
#ifdef DEBUG
		if (s != 0 && s->def == 0) {
			write (2, "D ", 2);
			write (2, namebuf, slen (namebuf));
			write (2, "\n", 1);
		}
#endif
		if (s != 0 && s->def == 0)
			return 1;
	}
	return 0;
}

/* ---- archives ---------------------------------------------------------- */

static int
arsize (p)
const char *p;
{
	int n;

	n = 0;
	while (p[0] == ' ')
		++p;
	while (p[0] >= '0' && p[0] <= '9') {
		n = n * 10 + (p[0] - '0');
		++p;
	}
	return n;
}

static dword	seloff[MAXSEL];
static int	nsel;

static int
isselected (off)
dword off;
{
	int i;

	for (i = 0; i < nsel; ++i)
		if (seloff[i] == off)
			return 1;
	return 0;
}

static void
process_archive (path)
const char *path;
{
	int fd;
	byte h[60];
	dword pos, size, end;
	int changed;
	word magic;

	fd = open (path, O_RDONLY);
	if (fd < 0)
		rderr (path);
	rd (fd, (dword) 0, h, 8);
	if (h[0] != '!' || h[1] != '<' || h[2] != 'a')
		fail ("bad archive");
	end = lseek (fd, 0L, SEEK_END);
	nsel = 0;
	do {
		changed = 0;
		pos = 8;
		while (pos + 60 <= end) {
			rd (fd, pos, h, 60);
			if (h[58] != '`' || h[59] != '\n')
				break;
			size = arsize (h + 48);
			pos += 60;
			if (!(h[0] == '/' && (h[1] == ' ' || h[1] == '/'))) {
				byte eh[52];
				word shnum, shent;
				dword shoff;
				rd (fd, pos, eh, 52);
				if (!isselected (pos) && pos + size <= end
				    && size >= 52 && eh[0] == 0x7f
				    && eh[1] == 'E') {
					shoff = rd32 (eh + 32);
					shnum = rd16 (eh + 48);
					shent = rd16 (eh + 46);
					if (defined_here (fd, pos, shnum,
							  shoff, shent)) {
#ifdef DEBUG
						write (2, "A ", 2);
						write (2, h, 16);
						write (2, "\n", 1);
#endif
						add_object (path, pos, size, fd);
						seloff[nsel++] = pos;
						changed = 1;
					}
				}
			}
			pos += size;
			if (size & 1)
				++pos;
		}
	} while (changed);
	close (fd);
}

/* ---- layout ------------------------------------------------------------ */

static word
alignup (v, a)
word v;
word a;
{
	if (a <= 1)
		return v;
	return (v + a - 1) & ~(a - 1);
}

static void
layout ()
{
	int i, j, c;
	struct gsym *s;
	word a;
	dword start;

	start = 0;
	for (j = OUT_TEXT; j <= OUT_DATA; ++j) {
		dots[j] = start;
		for (i = 1; i < nsecs; ++i) {
			if (secs[i].outsec != j)
				continue;
			a = secs[i].align;
			dots[j] = alignup (dots[j], a);
			secs[i].outaddr = dots[j];
			dots[j] += secs[i].size;
		}
		start = alignup (dots[j], 4);
	}
	/* user.x places *(COMMON) before *(.bss*) */
	dots[OUT_BSS] = start;
	for (i = 1; i < nsyms; ++i) {
		s = &syms[i];
		if (s->def != 2)
			continue;
		dots[OUT_BSS] = alignup (dots[OUT_BSS], s->align);
		s->addr = dots[OUT_BSS];
		dots[OUT_BSS] += s->size;
	}
	for (i = 1; i < nsecs; ++i) {
		if (secs[i].outsec != OUT_BSS)
			continue;
		a = secs[i].align;
		dots[OUT_BSS] = alignup (dots[OUT_BSS], a);
		secs[i].outaddr = dots[OUT_BSS];
		dots[OUT_BSS] += secs[i].size;
	}
	for (i = 1; i < nsyms; ++i) {
		s = &syms[i];
		if (s->def != 1)
			continue;
		if (s->sec == SHN_ABS)
			s->addr = s->value;
		else if (s->sec < 0xff00) {
			c = objs[s->obj].secbase + s->sec;
			s->addr = secs[c].outaddr + s->value;
		}
	}
	for (i = 0; i < ndefsyms; ++i)
		add_abs (defsyms[i].name, defsyms[i].value);
	add_abs ("__brk", dots[OUT_BSS]);
	add_abs ("__brk_end", (word) (0x10000 - stacksize));
	if (dots[OUT_BSS] > 0x10000 - stacksize)
		fail ("not enough room for stack");
}

/* ---- writing and relocations ------------------------------------------- */

static int
outsec (obj, sec)
int obj, sec;
{
	return secs[objs[obj].secbase + sec].outsec;
}

static void
write_sections (fdout)
int fdout;
{
	int i, n, b, fd;
	dword left, off;
	struct objinfo *o;

	for (i = 1; i < nsecs; ++i) {
		if (secs[i].outsec == OUT_NONE || secs[i].type == SHT_NOBITS)
			continue;
		if (secs[i].size == 0)
			continue;
		o = &objs[secs[i].obj];
		fd = open (nameget (o->path), O_RDONLY);
		if (fd < 0)
			rderr (nameget (o->path));
		left = secs[i].size;
		off = 0;
		while (left != 0) {
			n = left > IOBUF ? IOBUF : (int) left;
			rd (fd, o->base + secs[i].off + off, inbuf, n);
			/* bss never reaches here; the address is the file offset */
			wr (fdout, secs[i].outaddr + off, inbuf, n);
			off += n;
			left -= n;
		}
		close (fd);
	}

}

static void
relocate (obj, fd, fdout)
int obj, fd, fdout;
{
	struct secinfo *sh, *ts, *symsec, *strsec;
	struct objinfo *o;
	struct gsym *g;
	byte e[16], b[2];
	dword j, a, p, roff;
	int i, si, st, symidx, type;
	word shndx, s;
	char *str;

	o = &objs[obj];
	if (find_symtab (obj, &si, &st) != 0)
		return;
	symsec = &secs[o->secbase + si];
	strsec = &secs[o->secbase + st];
	for (i = 0; i < o->shnum; ++i) {
		sh = &secs[o->secbase + i];
		if (sh->type != SHT_REL || sh->info >= o->shnum)
			continue;
		ts = &secs[o->secbase + sh->info];
		if (ts->outsec == OUT_NONE)
			continue;
		for (j = 0; j < sh->size / 8; ++j) {
			rd (fd, o->base + sh->off + j * 8, e, 8);
			type = rd32 (e + 4) & 0xff;
			symidx = rd32 (e + 4) >> 8;
			roff = rd32 (e);
			if (type != 20 && type != 21)
				fail ("unsupported relocation type");
			if (roff + 2 > ts->size)
				fail ("relocation outside section");
			rd (fd, o->base + ts->off + roff, b, 2);
			a = rd16 (b);
			rd (fd, o->base + symsec->off + (dword) symidx * 16,
			    e, 16);
			shndx = rd16 (e + 14);
			if (shndx == SHN_ABS)
				s = rd32 (e + 4);
			else if (shndx == SHN_UNDEF || shndx == SHN_COMMON) {
				str = namebuf;
				symname (fd, o->base, strsec, rd16 (e), str);
				g = symfind (str);
				if (g == 0 || g->def == 0)
					errx (1, "ld: undefined reference to '%s'",
					      str);
				s = g->addr;
			} else {
				s = secs[o->secbase + shndx].outaddr
				    + rd32 (e + 4);
			}
			p = ts->outaddr + roff;
			if (type == 20)
				wr16 (b, (word) (s + a));
			else
				wr16 (b, (word) (s + a - p));
			wr (fdout, p, b, 2);
		}
	}
}

/* ---- driver ------------------------------------------------------------ */

static void
usage ()
{
	errx (1, "usage: ld -o out [-mstack=N] [-T script] file...");
}

static char *
findlib (name)
const char *name;
{
	int i, fd;
	char *d;

	for (i = 0; i < ndirs; ++i) {
		d = ldirs[i];
		scopy (pathbuf, d);
		if (slen (pathbuf) != 0 && pathbuf[slen (pathbuf) - 1] != '/')
			scopy (pathbuf + slen (pathbuf), "/");
		scopy (pathbuf + slen (pathbuf), "lib");
		scopy (pathbuf + slen (pathbuf), name);
		scopy (pathbuf + slen (pathbuf), ".a");
		fd = open (pathbuf, O_RDONLY);
		if (fd >= 0) {
			close (fd);
			return pathbuf;
		}
	}
	errx (1, "ld: cannot find library: %s", name);
	return 0;
}

int
main (argc, argv)
int	 argc;
char	**argv;
{
	int i, fd;
	char *objargs[MAXOBJ];
	int nobjargs;
	char *ars[MAXAR];
	int narargs;
	char *p;

	nobjargs = 0;
	narargs = 0;
	ldirs[0] = "lib/libc";
	ldirs[1] = "lib";
	ldirs[2] = ".";
	ndirs = 3;
	for (i = 1; i < argc; ++i) {
		p = argv[i];
		if (p[0] != '-') {
			if (slen (p) > 2 && strcmp (p + slen (p) - 2, ".a") == 0) {
				if (narargs >= MAXAR)
					fail ("too many archives");
				ars[narargs++] = p;
			} else {
				if (nobjargs >= MAXOBJ)
					fail ("too many objects");
				objargs[nobjargs++] = p;
			}
			continue;
		}
		if (strcmp (p, "-o") == 0 && i + 1 < argc)
			outpath = argv[++i];
		else if (strncmp (p, "-o", 2) == 0 && p[2])
			outpath = p + 2;
		else if (strncmp (p, "-mstack=", 8) == 0)
			stacksize = (word) atoi (p + 8);
		else if (strncmp (p, "-Wl,--defsym,STACK_SIZE=", 24) == 0)
			stacksize = (word) atoi (p + 24);
		else if (strncmp (p, "--defsym", 8) == 0) {
			char *q;
			if (p[8] == '=')
				q = p + 9;
			else if (p[8] == 0 && i + 1 < argc)
				q = argv[++i];
			else
				continue;
			if (ndefsyms < MAXDEF) {
				scopy (pathbuf, q);
				q = strchr (pathbuf, '=');
				if (q != 0) {
					*q++ = 0;
					defsyms[ndefsyms].name =
					    nameget (nameadd (pathbuf));
					defsyms[ndefsyms].value =
					    (word) atoi (q);
					++ndefsyms;
				}
			}
		} else if (strncmp (p, "-l:", 3) == 0) {
			if (narargs >= MAXAR)
				fail ("too many archives");
			scopy (arstore[narargs], p + 3);
			arlib[narargs] = 0;
			ars[narargs] = arstore[narargs];
			++narargs;
		} else if (p[0] == '-' && p[1] == 'l') {
			if (narargs >= MAXAR)
				fail ("too many archives");
			scopy (arstore[narargs], p + 2);
			arlib[narargs] = 1;
			ars[narargs] = arstore[narargs];
			++narargs;
		} else if (strcmp (p, "-L") == 0 && i + 1 < argc) {
			if (ndirs < 8)
				ldirs[ndirs++] = argv[++i];
		} else if (strncmp (p, "-L", 2) == 0) {
			if (ndirs < 8)
				ldirs[ndirs++] = p + 2;
		} else if (strcmp (p, "-T") == 0)
			++i;
		else if (strncmp (p, "-T", 2) == 0)
			;
		else if (strcmp (p, "-s") == 0 || strcmp (p, "-v") == 0
			 || strcmp (p, "-nostdlib") == 0
			 || strcmp (p, "-r") == 0)
			;
		else if (strncmp (p, "-Wl,", 4) == 0)
			;
		else
			;		/* ignore unknown options */
	}

	if (nobjargs == 0 && narargs == 0)
		usage ();

	for (i = 0; i < narargs; ++i) {
		if (!arlib[i])
			continue;
		scopy (arstore[i], findlib (arstore[i]));
	}

	/* symbol 0 is the null symbol */
	nsyms = 1;
	syms[0].name = nameadd ("");
	syms[0].def = 0;

	for (i = 0; i < nobjargs; ++i) {
		fd = open (objargs[i], O_RDONLY);
		if (fd < 0)
			rderr (objargs[i]);
		add_object (objargs[i], (dword) 0, (dword) lseek (fd, 0L, SEEK_END), fd);
		close (fd);
	}
	for (i = 0; i < narargs; ++i)
		process_archive (ars[i]);

	layout ();

	fd = open (outpath, O_WRONLY | O_CREAT | O_TRUNC, 0644);
	if (fd < 0)
		rderr (outpath);
	write_sections (fd);
	for (i = 0; i < nobjs; ++i) {
		int infd;
		infd = open (nameget (objs[i].path), O_RDONLY);
		if (infd < 0)
			rderr (nameget (objs[i].path));
		relocate (i, infd, fd);
		close (infd);
	}
	close (fd);
	return 0;
}
