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

#ifndef ATTR_H
#define ATTR_H

#include <stddef.h>

struct attr
{
	char *name;
	char *value;
	struct attr *next;
};

struct attr_map {
	char *name;
	int flags;
};

enum ATTR_FLAG {
	ATTR_FLAG_GLOBAL = (1 << 0),
	ATTR_FLAG_EVENT = (1 << 1),
};

struct attr *attr_get(struct attr *, const char *);
void attr_set(struct attr **, const char *, const char *);
int attr_has(struct attr *, const char *);

size_t attr_size(struct attr *);

#endif
