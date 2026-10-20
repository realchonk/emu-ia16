#include <stdbool.h>
#include <stdlib.h>
#include <sys/queue.h>
#include <unistd.h>

#define MAX(a, b) ((a) > (b) ? (a) : (b))

struct map {
	size_t		 len;
	TAILQ_ENTRY (map) link;
};

TAILQ_HEAD (map_head, map);

static struct map_head free_list = TAILQ_HEAD_INITIALIZER (free_list);

#define map_end(m) ((void *)((char *)(m) + (m)->len))

void *
malloc (num)
size_t num;
{
	struct map	*m, *n, *l = NULL;
	void		*ptr;
	size_t		 anum;

	anum = MAX (num + sizeof (size_t), sizeof (struct map));

	/* try finding an already free block */
	TAILQ_FOREACH (m, &free_list, link) {
		if (m->len >= anum)
			break;
		l = m;
	}

	if (m == NULL) {
		/* if the free last block is at the brk, just extend it */
		if (l != NULL && map_end (l) == sbrk (0)) {
			if (sbrk (anum - l->len) == (void *)-1)
				return NULL;
			TAILQ_REMOVE (&free_list, l, link);
			l->len = anum;
			return (char *)l + sizeof (size_t);
		}

		/* if no block was found, allocate by extending the program break */
		ptr = sbrk (anum);
		if (ptr == (void *)-1)
			return NULL;
		*((size_t *)ptr) = anum;
		return (char *)ptr + sizeof (size_t);
	}

	/*
	 * If the block found is big enough to also hold a free block,
	 * split it and let the remainder n take m's place in the free
	 * list.  Otherwise hand out all of m and drop it from the list.
	 */
	if (m->len >= anum + sizeof (struct map)) {
		n = (struct map *)((char *)m + anum);
		n->len = m->len - anum;
		TAILQ_INSERT_AFTER (&free_list, m, n, link);
		TAILQ_REMOVE (&free_list, m, link);
	} else {
		anum = m->len;
		TAILQ_REMOVE (&free_list, m, link);
	}

	m->len = anum;
	return (char *)m + sizeof (size_t);
}

static bool
try_merge (p, n)
struct map *p, *n;
{
	if (p == NULL || n == NULL || map_end (p) != n)
		return false;

	p->len += n->len;
	TAILQ_REMOVE (&free_list, n, link);
	return true;
}

void
free (ptr)
void *ptr;
{
	struct map	*m, *i, *p = NULL;

	m = (struct map *)((char *)ptr - sizeof (size_t));

	/* try finding a place to insert into the free list */
	for (i = TAILQ_FIRST (&free_list); i != NULL && m > i; p = i, i = TAILQ_NEXT (i, link));

	if (p == NULL)
		TAILQ_INSERT_HEAD (&free_list, m, link);
	else
		TAILQ_INSERT_AFTER (&free_list, p, m, link);

	if (try_merge (p, m)) {
		try_merge (p, i);
	} else {
		try_merge (m, i);
	}
}

/* free all heap memory in one go */
void
freeall ()
{
	extern char __brk[];
	brk (__brk);
	TAILQ_INIT (&free_list);
}
