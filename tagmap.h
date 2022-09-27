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

#ifndef TAGMAP_H
#define TAGMAP_H

#include "tags.h"

typedef enum tag_flags {
	TAG_EMPTY = (1 << 0),
	TAG_OPTIONAL_CLOSE = (1 << 1),
	TAG_BLOCK = (1 << 2),
	TAG_SPECIAL = (1 << 3),
	TAG_HEADING = (1 << 4),
	TAG_FORMAT = (1 << 5)
} TAG_FLAGS;

struct tag {
	char *name;
	unsigned char flags;
};

int			 tagmap_id(const char *);
const struct tag	*tagmap(int);

#endif
