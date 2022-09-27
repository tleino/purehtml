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

#include "tagmap.h"

#include <string.h>
#include <assert.h>
#include <err.h>

#include "tags.c"

#define TAGMAP_SZ 1024

const struct tag *
tagmap(int i)
{
	assert(i >= 0 && i < TAGMAP_SZ);

#if 0
	warnx("tagmap returns tag=%s flags %d\n", tags[i].name, tags[i].flags);
#endif
	return &tags[i];
}

int
tagmap_id(const char *tag)
{
	size_t	 i;
	size_t	 sz;
	size_t	 prime;
	size_t	 modulo;
	size_t	 addr;

	prime = 104729;
	modulo = 1048576;
	sz = strlen(tag);
	addr = 0;
	for (i = 0; i < sz; i++) {
		addr = addr + tag[i];
		addr *= prime;
		addr %= modulo;
	}

	i = addr % TAGMAP_SZ;
	if (tags[i].name == NULL)
		return TAG_CUSTOM_TAG;

	while (tags[i].name != NULL &&
	    strcmp(tags[i].name, tag) != 0) {
		addr++;
		i = addr % TAGMAP_SZ;
	}
	if (tags[i].name == NULL)
		return TAG_CUSTOM_TAG;

#if 0
	warnx("tagmap '%s' i=%d, i", tag, i);
#endif

	return i;
}
