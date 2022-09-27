/*
 * ISC License
 *
 * Copyright (c) 2022, Tommi Leino <namhas@gmail.com>
 *
 * Permission to use, copy, modify, and/or distribute this software for any
 * purpose with or without fee is hereby granted, provided that the above
 * copyright notice and this permission notice appear in all copies.
 *
 * THE SOFTWARE IS PROVIDED "AS IS" AND THE AUTHOR DISCLAIMS ALL WARRANTIES
 * WITH REGARD TO THIS SOFTWARE INCLUDING ALL IMPLIED WARRANTIES OF
 * MERCHANTABILITY AND FITNESS. IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR
 * ANY SPECIAL, DIRECT, INDIRECT, OR CONSEQUENTIAL DAMAGES OR ANY DAMAGES
 * WHATSOEVER RESULTING FROM LOSS OF USE, DATA OR PROFITS, WHETHER IN AN
 * ACTION OF CONTRACT, NEGLIGENCE OR OTHER TORTIOUS ACTION, ARISING OUT OF
 * OR IN CONNECTION WITH THE USE OR PERFORMANCE OF THIS SOFTWARE.
 */

#include "ostack.h"

#include <stdlib.h>
#include <err.h>
#include <assert.h>

/*
 * Open elements stack.
 */
static struct elem **_ostack;
static size_t _ostack_alloc;
static size_t _ostack_depth;

void
ostack_push(struct elem *node)
{
	if (_ostack_depth == _ostack_alloc) {
		_ostack_alloc += 4;
		_ostack = realloc(_ostack, _ostack_alloc * sizeof(struct domnode *));
		if (_ostack == NULL)
			err(1, "realloc ostack");
	}

	assert(_ostack != NULL);
	_ostack[_ostack_depth++] = node;
}

#include "elem.h"
#include <err.h>
struct elem *
ostack_prev(struct elem *elem)
{
	size_t i;
	size_t sz;

	sz = ostack_depth();
	for (i = sz; i >= 1; i--)  {
		if (_ostack[i-1] == elem && i >= 2) {
			return _ostack[i-2];
		} else
			break;
	}
	return NULL;
}

struct elem *
ostack_pop()
{
	if (_ostack_depth > 0) {
		_ostack_depth--;
		return _ostack[_ostack_depth];
	}
	if (_ostack_depth == 0 && _ostack_alloc > 0) {
		free(_ostack);
		_ostack = NULL;
		_ostack_alloc = 0;
	}

	return NULL;
}

struct elem *
ostack_peek_at(size_t depth)
{
	if (_ostack_depth == 0 || depth > _ostack_depth || depth < 1)
		return NULL;

	return _ostack[depth-1];
}

struct elem *
ostack_peek()
{
	return ostack_peek_at(_ostack_depth);
}

size_t
ostack_depth()
{
	return _ostack_depth;
}

#ifdef TEST
#include <stdio.h>
int
main(int argc, char **argv)
{
	struct elem elem1, elem2;


	ostack_push(&elem1);
	ostack_push(&elem2);
	assert(ostack_pop() == &elem2);
	assert(ostack_pop() == &elem1);
	assert(ostack_pop() == NULL);
	return 0;
}
#endif
