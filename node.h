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

#ifndef NODE_H
#define NODE_H

#include <stddef.h>

typedef enum nodetype {
	NODE_ELEM,
	NODE_CDATA,
	NODE_DOCUMENT
} T_NODE;

struct node {
	T_NODE	type;

	union {
		struct elem	*elem;
		struct cdata	*cdata;
		struct document	*document;
	} u;

	/*
	 * Optional nodes for DOM construction.
	 */
	struct node *parent;
	struct node *first;
	struct node *last;
	struct node *next;
	struct node *prev;
};

struct node	*node_create_from_elem(struct elem *);
struct node	*node_create_from_cdata(struct cdata *);
struct node	*node_create_from_document(struct document *);

size_t node_size(struct node *);
void node_free(struct node *);

#endif
