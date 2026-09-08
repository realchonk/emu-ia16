#include <stdbool.h>
#include <stdlib.h>
#include <unistd.h>
#include <errno.h>

#define MAX(a, b) ((a) > (b) ? (a) : (b))

struct map {
	size_t		 len;
	struct map	*next, *prev;
};

#define map_end(m) ((void *)((char *)(m) + (m)->len))

static struct map *free_list = NULL;

static struct map *
find_free (num, last)
struct map	**last;
size_t		  num;
{
	struct map *m;

	*last = NULL;
	for (m = free_list; m != NULL; *last = m, m = m->next) {
		if (m->len >= num)
			return m;
	}

	return NULL;
}

void *
malloc (num)
size_t num;
{
	struct map	*m, *n, *l;
	void		*ptr;
	size_t		 anum;

	anum = MAX (num + sizeof (size_t), sizeof (struct map) + 2);

	/* try finding an already free block */
	m = find_free (anum, &l);
	if (m == NULL) {
		/* if the free last block is at the brk, just extend it */
		if (l != NULL && map_end (l) == sbrk (0)) {
			if (sbrk (anum - l->len) == (void *)-1)
				return NULL;
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

	/* if the block found is bigger than requested, split it up */
	if (m->len >= num + sizeof (struct map) * 2 + 4) {
		anum = num + sizeof (struct map);
		n = (void *)((char *)m + anum);
		n->prev = m->prev;
		n->next = n->next;
		n->len = m->len - anum;
	} else {
		anum = m->len;
		n = m->next;
	}

	if (m == free_list)
		free_list = n;
	if (n->prev != NULL)
		n->prev->next = n;
	if (n->next != NULL)
		n->next->prev = n;

	m->len = anum;
	return (char *)m + sizeof (size_t);
}

static bool
try_merge (p, n)
struct map *p, *n;
{
	if (map_end (p) != n)
		return false;

	p->len += n->len;
	p->next = n->next;
	if (p->next != NULL)
		p->next->prev = p;
	return true;
}

void
free (ptr)
void *ptr;
{
	struct map	*m, *i, *p = NULL;
	size_t		 len;

	m = (struct map *)((char *)ptr - sizeof (size_t));
	len = m->len;

	m->next = NULL;
	m->prev = NULL;
	m->len = len;

	/* try finding a place to insert into the free list */
	for (i = free_list; i != NULL && m > i; p = i, i = i->next);

	if (i == NULL) {
		if (p == NULL) {
			free_list = m;
			return;
		}
		p->next = m;
		m->prev = p;
		try_merge (p, m);
		return;
	}

	m->prev = p;
	m->next = i;
	i->prev = m;
	if (p == NULL) {
		free_list = m;
		try_merge (m, i);
		return;
	}

	p->next = m;
	if (try_merge (p, m)) {
		try_merge (p, i);
	} else {
		try_merge (m, i);
	}
	
}
