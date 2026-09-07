#include <_varargs.h>
#include <stdio.h>

static int
putstr (f, p, s)
void		(*f)();
void		 *p;
const char	 *s;
{
	int n;
	for (n = 0; *s != '\0'; ++s, ++n)
		f (p, *s);
	return n;
}

static const char digits[] = "0123456789ABCDEF";

static int
printu (f, p, n, base)
void	(*f)();
void	 *p;
unsigned  n, base;
{
	int c = 0;

	if (n >= base) {
		c += printu (f, p, n / base, base);
		n %= base;
	}

	f (p, digits[n]);
	return c + 1;
}

static int
printd (f, p, n, base)
void	(*f)();
void	 *p;
int	  n;
unsigned  base;
{
	int c = 0;
	if (n < 0) {
		f (p, '-');
		n = -n;
		++c;
	}
	return c + printu (f, p, n, base);
}

static int
printp (f, p, x)
void	(*f)();
void	 *p, *x;
{
	unsigned i = (unsigned)x;
	f (p, '0');
	f (p, 'x');
	f (p, digits[i >> 12]);
	f (p, digits[(i >> 8) & 0xf]);
	f (p, digits[(i >> 4) & 0xf]);
	f (p, digits[i & 0xf]);
	return 6;
}

int
_vprintf (f, p, fmt, ap)
void		(*f)();
void		*p;
const char	 *fmt;
va_list		  ap;
{
	int n;

	for (n = 0; *fmt != '\0'; ) {
		if (*fmt != '%') {
			f (p, *fmt++);
			++n;
			continue;
		}

		++fmt;

		switch (*fmt++) {
		case '%':
			f (p, '%');
			++n;
			break;
		case 'c':
			f (p, va_arg (ap, int));
			++n;
			break;
		case 's':
			n += putstr (f, p, va_arg (ap, const char *));
			break;
		case 'x':
			n += printu (f, p, va_arg (ap, unsigned), 16);
			break;
		case 'u':
			n += printu (f, p, va_arg (ap, unsigned), 10);
			break;
		case 'd':
			n += printd (f, p, va_arg (ap, int), 10);
			break;
		case 'p':
			n += printp (f, p, va_arg (ap, void *));
			break;
		default:
			f (p, '%');
			f (p, fmt[-1]);
			n += 2;
			break;
		}
	}

	return n;
}
