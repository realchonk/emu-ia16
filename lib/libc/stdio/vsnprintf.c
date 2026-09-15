#include <_varargs.h>
#include <stdio.h>

extern int _vprintf ();

struct context {
	char	*buf;
	size_t	i, n;
};

static void
snputc (p, c)
void	*p;
int	 c;
{
	struct context	*ctx = p;

	if (ctx->i >= ctx->n)
		return;

	ctx->buf[ctx->i++] = c;
}

int
vsnprintf (s, num, fmt, ap)
char		*s;
size_t		 num;
const char	*fmt;
va_list		 ap;
{
	struct context	ctx;
	int		n;

	ctx.buf	= s;
	ctx.n	= num != 0 ? num - 1 : 0;
	ctx.i	= 0;

	n = _vprintf (snputc, &ctx, fmt, ap);
	if (num != 0)
		ctx.buf[ctx.i] = '\0';
	return n;
}

