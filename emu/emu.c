/* Copyright (c) 2026 Robert Clausecker <fuz@fuz.ooo>
 *
 * The original version of this program
 * was written by Robert Clausecker.
 * 
 * Modifications by Benjamin Stürz <benni@stuerz.xyz>:
 * - Port to Linux
 * - Push argv to the guest
 * - Add more syscalls
 * */
#ifdef __linux__
#define	_GNU_SOURCE	/* MAP_32BIT */
#include <sys/types.h>
#include <sys/mman.h>
#include <sys/syscall.h>
#include <asm/ldt.h>
#include <errno.h>
#define	_write	write
#elif defined(__FreeBSD__)
#include <sys/types.h>
#include <sys/mman.h>
#include <machine/param.h>
#include <machine/sysarch.h>
#include <machine/segments.h>
#endif
#include <fcntl.h>
#include <assert.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <stdnoreturn.h>
#include <string.h>
#include <unistd.h>

/* labels defined in shim.S */
extern const char shim32[];
extern const char host_cs, host_ds, shim32_len;

/* stack pointer for syscalls from 16 bit mode */
struct {
	uint32_t esp;
	uint16_t ss;
} stack64;
char *text;

#define STACKSIZ 8192
uintptr_t stack[STACKSIZ];

#ifdef __linux__
/* add a descriptor to the LDT and return its selector */
static int
ldt_add_seg(uint32_t base, uint32_t lim, int exec, int d32)
{
	static int next = 1;	/* entry 0 is not usable */
	struct user_desc ud;

	if (next >= LDT_ENTRIES) {
		errno = ENOSPC;
		return (-1);
	}

	memset(&ud, 0, sizeof(ud));
	ud.entry_number = next++;
	ud.base_addr = base;
	ud.limit = lim;
	ud.seg_32bit = d32;
	ud.contents = exec ? MODIFY_LDT_CONTENTS_CODE : MODIFY_LDT_CONTENTS_DATA;
	ud.read_exec_only = 0;
	ud.limit_in_pages = 0;	/* byte granularity */
	ud.seg_not_present = 0;
	ud.lm = 0;

	if (syscall(SYS_modify_ldt, 1, &ud, sizeof(ud)) == -1)
		return (-1);

	return ((int)(ud.entry_number << 3) | 4 | 3);
}
#elif defined(__FreeBSD__)
/* this function is unfortunately not defined on amd64 */
int
i386_set_ldt(int start, union descriptor *descs, int num)
{
	struct i386_ldt_args p;

	p.start = start;
	p.descs = &descs->sd;
	p.num   = num;

	return sysarch(I386_SET_LDT, &p);
}

/* add a descriptor to the LDT and return its selector */
static int
ldt_add_seg(uint32_t base, uint32_t lim, int exec, int d32)
{
	int res;
	union descriptor desc;

	USD_SETBASE(&desc.sd, base);
	USD_SETLIMIT(&desc.sd, lim);
	desc.sd.sd_type = exec ? SDT_MEMER : SDT_MEMRW;
	desc.sd.sd_dpl = 3;
	desc.sd.sd_p = 1;
	desc.sd.sd_long = 0;
	desc.sd.sd_def32 = d32;
	desc.sd.sd_gran = 0; /* byte granularity */
	res = i386_set_ldt(LDT_AUTO_ALLOC, &desc, 1);
	if (res == -1)
		return (-1);

	return (LSEL(res, 3));
}
#endif

/* allocate a segment for the 32 bit shim and set it up */
int
shim32_setup(void)
{
	char *shim32seg;
	int shimsel;

	shim32seg = mmap(NULL, sysconf(_SC_PAGESIZE), PROT_READ|PROT_WRITE, MAP_ANON|MAP_PRIVATE|MAP_32BIT, -1, 0);
	if (shim32seg == MAP_FAILED)
		return (0);

	memcpy(shim32seg, shim32, (size_t)&shim32_len);
	asm ("movw %%cs, %0" : "=m"(shim32seg[&host_cs - shim32]));
#ifdef __linux__
	/* on Linux, %ds is the null selector, so take it from %ss instead */
	asm ("movw %%ss, %0" : "=m"(shim32seg[&host_ds - shim32]));
#elif defined(__FreeBSD__)
	asm ("movw %%ds, %0" : "=m"(shim32seg[&host_ds - shim32]));
#endif

	if (mprotect(shim32seg, sysconf(_SC_PAGESIZE), PROT_READ|PROT_EXEC) == -1)
		return (0);

	assert((uintptr_t)shim32seg <= UINT32_MAX);
	shimsel = ldt_add_seg((uint32_t)(uintptr_t)shim32seg,
	    (uint32_t)(uintptr_t)&shim32_len - 1, 1, 1);
	if (shimsel == -1)
		return (0);

	return (shimsel);
}

/* allocate text and data segments for 16 bit program image */
int
setup_text(char **text, int *cs, int *ds)
{
	*text = mmap(NULL, 65536, PROT_READ|PROT_WRITE|PROT_EXEC, MAP_ANON|MAP_PRIVATE|MAP_32BIT, -1, 0);
	if (*text == MAP_FAILED)
		return (-1);

	assert((uintptr_t)*text <= UINT32_MAX);

	*cs = ldt_add_seg((uint32_t)(uintptr_t)*text, 65535, 1, 0);
	if (*cs == -1)
		return (-1);

	*ds = ldt_add_seg((uint32_t)(uintptr_t)*text, 65535, 0, 0);
	if (*ds == -1)
		return (-1);

	return (0);
}

/*
 * Build the initial stack for the 16 bit program:
 *
 *	sp	argc
 *	sp+2	argv[0] .. argv[argc-1], NULL
 *		argument strings, packed at the top of the segment
 */
static uint16_t
setup_args(char **argv)
{
	uint16_t sp;
	int argc, i;

	for (argc = 0; argv[argc] != NULL; argc++)
		;

	/* copy the strings to the top of the address space */
	sp = 0;
	for (i = argc - 1; i >= 0; i--) {
		size_t len = strlen(argv[i]) + 1;

		sp -= len;
		memcpy(text + sp, argv[i], len);
		argv[i] = (char *)(uintptr_t)sp; /* now a segment offset */
	}

	sp &= ~1; /* word align */

	/* NULL terminated argument vector */
	sp -= 2;
	*(uint16_t *)(text + sp) = 0;
	for (i = argc - 1; i >= 0; i--) {
		sp -= 2;
		*(uint16_t *)(text + sp) = (uint16_t)(uintptr_t)argv[i];
	}

	/* argument count */
	sp -= 2;
	*(uint16_t *)(text + sp) = argc;

	if (sp < 0x8000) {
		fprintf(stderr, "arguments too long\n");
		exit(EXIT_FAILURE);
	}

	return (sp);
}

/* enter 16 bit mode */
noreturn void
enter16(int cs, int ds, int shimsel, uint32_t sp)
{
	static struct {
		uint16_t ip, cs;
	} entry = { 0, 0 };

	entry.cs = cs;

	asm volatile (
		"mov	%0, %%ds\n\t"
		"mov	%0, %%ss\n\t"
		"mov	%4, %%esp\n\t"
		"ljmpw	*%1"
	::
		"r"(ds), "m"(entry), "a"(shimsel), "c"(0), "r"(sp));

	__builtin_unreachable();
}

int main(int argc, char *argv[])
{
	ssize_t	 n, off;
	int	 cs, ds, shimsel, aoutfd;
	char	*prog;

	if (argc < 2) {
		fputs ("usage: emu prog [args...]\n", stderr);
		return 1;
	}
	prog = argv[1];

	aoutfd = open(prog, O_RDONLY|O_CLOEXEC);
	if (aoutfd == -1) {
		perror(prog);
		return (EXIT_FAILURE);
	}

	shimsel = shim32_setup();
	if (shimsel == 0) {
		perror("shim32_setup");
		return (EXIT_FAILURE);
	}

	stack64.esp = (uint32_t)(uintptr_t)&stack[STACKSIZ-1];
	asm ("mov %%ss, %0" : "=m"(stack64.ss));

	if (setup_text(&text, &cs, &ds) != 0) {
		perror("setup_text");
		return (EXIT_FAILURE);
	}

	for (off = 0, n = 1; off < 65536 && n > 0; off += n)
		n = read(aoutfd, text + off, 65536 - off);

	if (n == -1) {
		perror(prog);
		return (EXIT_FAILURE);
	}

	enter16(cs, ds, shimsel, setup_args(argv + 1));
}

static int
xoflags (int oflags)
{
	int nflags = 0;

	switch (oflags & 3) {
	case 0:
		nflags = O_RDONLY;
		break;
	case 1:
		nflags = O_WRONLY;
		break;
	case 2:
		nflags = O_RDWR;
		break;
	default:
		return 0;
	}

	if (oflags & 0x0008)
		nflags |= O_APPEND;
	if (oflags & 0x0200)
		nflags |= O_CREAT;
	if (oflags & 0x0400)
		nflags |= O_TRUNC;
	if (oflags & 0x0800)
		nflags |= O_EXCL;

	return nflags;
}

int
sysentry(int ss, uint32_t esp, int no)
{
	char		*linsp;
	uint16_t	*args;

	linsp = text + (esp & 0xffff);
	args = (uint16_t *)(linsp + 6);

	switch (no) {
	case 0:
		return sysentry (ss, esp + 2, args[0]);
	case 1:
		_exit(args[0]);
	case 2:
		return _write (args[0], text + args[1], args[2]);
	case 3:
		return read (args[0], text + args[1], args[2]);
	case 4:
		return close (args[0]);
	case 5:
		return lseek (args[0], args[1] | (args[2] << 16), args[3]);
	case 6:
		return unlink (text + args[0]);
	case 7:
		return open (text + args[0], xoflags (args[1]), args[2]);
	case 8:
		return creat (text + args[0], args[1]);

	default:
		dprintf(STDERR_FILENO, "syscall%d(%d, %d, %d, %d)\n", no, args[0], args[1], args[2], args[3]);
		return (0);
	}
}
