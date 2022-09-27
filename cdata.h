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

#ifndef CDATA_H
#define CDATA_H

#include "util.h"

typedef enum cdatatype {
	CDATA_TEXT,
	CDATA_COMMENT,
} T_CDATA;

struct node;
struct str;

struct cdata {
	T_CDATA		 type;
	struct str	 data;

	struct node	*node;	/* back reference, can be NULL */
};

struct cdata	*cdata_create(T_CDATA);
void		 cdata_add(struct cdata *, const char *);
void		 cdata_free(struct cdata *);
size_t		 cdata_size(struct cdata *);

#endif
