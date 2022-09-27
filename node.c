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

#include "node.h"
#include "elem.h"
#include "cdata.h"
#include "document.h"

#include <stdlib.h>
#include <err.h>
#include <assert.h>

static struct node	*node_create(T_NODE type);

struct node *
node_create_from_cdata(struct cdata *cdata)
{
	struct node	*node;

	assert(cdata != NULL);

	node = node_create(NODE_CDATA);
	node->u.cdata = cdata;
	cdata->node = node;

	return node;
}

struct node *
node_create_from_elem(struct elem *elem)
{
	struct node	*node;

	assert(elem != NULL);

	node = node_create(NODE_ELEM);
	node->u.elem = elem;
	elem->node = node;

	return node;
}

static size_t
node_free_internal(struct node *node, int want_sum)
{
	size_t sum = sizeof(struct node);

	switch (node->type) {
	case NODE_ELEM:
		if (want_sum)
			sum += elem_size(node->u.elem);
		else
			elem_free(node->u.elem);
		break;
	case NODE_CDATA:
		if (want_sum)
			sum += cdata_size(node->u.cdata);
		else
			cdata_free(node->u.cdata);
		break;
	default:
		break;
	}

	if (!want_sum)
		free(node);

	return sum;
}

size_t
node_size(struct node *node)
{
	return node_free_internal(node, 1);
}

void
node_free(struct node *node)
{
	(void) node_free_internal(node, 0);
}

struct node *
node_create_from_document(struct document *document)
{
	struct node	*node;

	assert(document != NULL);

	node = node_create(NODE_DOCUMENT);
	node->u.document = document;
	document->node = node;

	return node;
}

static struct node *
node_create(T_NODE type)
{
	struct node	*node;

	node = calloc(1, sizeof(struct node));
	if (node == NULL)
		err(1, "calloc node");

	node->type = type;

	return node;
}
