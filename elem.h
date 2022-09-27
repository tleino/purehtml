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

#ifndef ELEM_H
#define ELEM_H

#include <stddef.h>

struct node;
struct token;
struct attr;

enum elem_ns {
	NS_HTML,
	NS_MATHML,
	NS_SVG,
	NS_XLINK,
	NS_XML,
	NS_XMLNS
};

struct elem {
	int		 tagid;
	char		*name;
	struct attr	*attr;
	struct node	*node;	/* back reference, can be NULL */
	int		 ns;
};

struct elem	*elem_create_from_token(struct token *token);
struct elem	*elem_create(const char *name);
int		 elem_has_attr(struct elem *, const char *);
const char	*elem_attr_value(struct elem *, const char *);
void		 elem_set_attr(struct elem *, const char *, const char *);

size_t elem_size(struct elem *);
void elem_free(struct elem *);

#endif
