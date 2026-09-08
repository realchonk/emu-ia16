#include <_varargs.h>
#include <stdio.h>

/* conversion flags */
#define F_LEFT	0x01	/* '-': left justify			*/
#define F_ZERO	0x02	/* '0': pad numbers with zeroes		*/
#define F_SIGN	0x04	/* '+': always print a sign		*/
#define F_SPACE	0x08	/* ' ': print a blank instead of '+'	*/
#define F_ALT	0x10	/* '#': alternate form			*/

/*
 * Argument length modifiers.  On ia16 short, int, size_t and ptrdiff_t
 * are all 16 bit wide, so h/hh/z/t read an int sized argument, while
 * l/ll/j read a long.
 */
#define L_INT	0
#define L_LONG	1

/* enough digits for the widest supported value in base 8 */
#define NUMBUF	24

static const char ldigits[] = "0123456789abcdef";
static const char udigits[] = "0123456789ABCDEF";

static void
padout (f, p, c, n)
void	(*f)();
void	 *p;
int	  c, n;
{
	int i;

	for (i = 0; i < n; ++i)
		f (p, c);
}

/*
 * Emit s (of length len) padded to width, honouring F_LEFT.
 */
static int
strout (f, p, s, len, flags, width)
void		(*f)();
void		 *p;
const char	 *s;
int		  len, flags, width;
{
	int i, pad;

	pad = width > len ? width - len : 0;

	if (!(flags & F_LEFT))
		padout (f, p, ' ', pad);
	for (i = 0; i < len; ++i)
		f (p, s[i]);
	if (flags & F_LEFT)
		padout (f, p, ' ', pad);

	return len + pad;
}

/*
 * Emit the unsigned value val in the given base, preceded by the
 * optional sign character and the optional prefix pfx ("0x", "0", ...).
 * prec is the minimum number of digits, or -1 if unspecified.
 */
static int
numout (f, p, val, sign, pfx, base, digits, flags, width, prec)
void		(*f)();
void		 *p;
unsigned long	  val;
int		  sign, base, flags, width, prec;
const char	 *pfx, *digits;
{
	char	buf[NUMBUF];
	int	len, plen, zeros, total, pad;

	len = 0;
	if (val != 0 || prec != 0) {
		do {
			buf[len++] = digits[val % base];
			val /= base;
		} while (val != 0 && len < NUMBUF);
	}

	for (plen = 0; pfx[plen] != '\0'; ++plen)
		;

	zeros = prec > len ? prec - len : 0;

	/* '0' is ignored when a precision or '-' is given */
	if ((flags & F_ZERO) && !(flags & F_LEFT) && prec < 0) {
		total = width - len - plen - (sign != 0);
		if (total > zeros)
			zeros = total;
	}

	total = len + zeros + plen + (sign != 0);
	pad = width > total ? width - total : 0;

	if (!(flags & F_LEFT))
		padout (f, p, ' ', pad);
	if (sign)
		f (p, sign);
	for (; *pfx != '\0'; ++pfx)
		f (p, *pfx);
	padout (f, p, '0', zeros);
	while (len > 0)
		f (p, buf[--len]);
	if (flags & F_LEFT)
		padout (f, p, ' ', pad);

	return total + pad;
}

int
_vprintf (f, p, fmt, ap)
void		(*f)();
void		*p;
const char	 *fmt;
va_list		  ap;
{
	int		 n, flags, width, prec, lmod, slen, base, sign, conv;
	const char	*s, *pfx, *digits;
	long		 sval;
	unsigned long	 uval;
	char		 c;

	n = 0;
	while (*fmt != '\0') {
		if (*fmt != '%') {
			f (p, *fmt++);
			++n;
			continue;
		}

		++fmt;

		/* flags */
		flags = 0;
		for (;; ++fmt) {
			if (*fmt == '-')
				flags |= F_LEFT;
			else if (*fmt == '0')
				flags |= F_ZERO;
			else if (*fmt == '+')
				flags |= F_SIGN;
			else if (*fmt == ' ')
				flags |= F_SPACE;
			else if (*fmt == '#')
				flags |= F_ALT;
			else
				break;
		}

		/* field width */
		width = 0;
		if (*fmt == '*') {
			++fmt;
			width = va_arg (ap, int);
			if (width < 0) {
				flags |= F_LEFT;
				width = -width;
			}
		} else {
			while (*fmt >= '0' && *fmt <= '9')
				width = width * 10 + (*fmt++ - '0');
		}

		/* precision */
		prec = -1;
		if (*fmt == '.') {
			++fmt;
			prec = 0;
			if (*fmt == '*') {
				++fmt;
				prec = va_arg (ap, int);
				if (prec < 0)
					prec = -1;
			} else {
				while (*fmt >= '0' && *fmt <= '9')
					prec = prec * 10 + (*fmt++ - '0');
			}
		}

		/* length modifier */
		lmod = L_INT;
		switch (*fmt) {
		case 'h':
			++fmt;
			if (*fmt == 'h')
				++fmt;
			break;
		case 'l':
			++fmt;
			if (*fmt == 'l')
				++fmt;
			lmod = L_LONG;
			break;
		case 'j':
			++fmt;
			lmod = L_LONG;
			break;
		case 'z':
		case 't':
			++fmt;
			break;
		default:
			break;
		}

		conv = *fmt++;
		switch (conv) {
		case '\0':
			/* trailing '%': stop here */
			f (p, '%');
			return n + 1;

		case '%':
			f (p, '%');
			++n;
			break;

		case 'c':
			c = va_arg (ap, int);
			n += strout (f, p, &c, 1, flags, width);
			break;

		case 's':
			s = va_arg (ap, const char *);
			if (s == NULL)
				s = "(null)";
			for (slen = 0; s[slen] != '\0'; ++slen)
				if (prec >= 0 && slen >= prec)
					break;
			n += strout (f, p, s, slen, flags, width);
			break;

		case 'p':
			uval = (unsigned long)(size_t)va_arg (ap, void *);
			if (prec < 0)
				prec = 2 * sizeof (void *);
			n += numout (f, p, uval, 0, "0x", 16, ldigits,
				     flags & ~F_ZERO, width, prec);
			break;

		case 'd':
		case 'i':
		case 'u':
		case 'o':
		case 'x':
		case 'X':
			base = 10;
			digits = ldigits;
			if (conv == 'o')
				base = 8;
			else if (conv == 'x' || conv == 'X')
				base = 16;
			if (conv == 'X')
				digits = udigits;

			sign = 0;
			if (conv == 'd' || conv == 'i') {
				sval = lmod == L_LONG
					   ? va_arg (ap, long)
					   : (long)va_arg (ap, int);
				if (sval < 0) {
					sign = '-';
					uval = -(unsigned long)sval;
				} else {
					uval = (unsigned long)sval;
					if (flags & F_SIGN)
						sign = '+';
					else if (flags & F_SPACE)
						sign = ' ';
				}
			} else {
				uval = lmod == L_LONG
					   ? va_arg (ap, unsigned long)
					   : (unsigned long)va_arg (ap, unsigned);
			}

			pfx = "";
			if (flags & F_ALT) {
				if (base == 8)
					pfx = "0";
				else if (base == 16 && uval != 0)
					pfx = conv == 'X' ? "0X" : "0x";
			}

			n += numout (f, p, uval, sign, pfx, base, digits,
				     flags, width, prec);
			break;

		default:
			f (p, '%');
			f (p, conv);
			n += 2;
			break;
		}
	}

	return n;
}
