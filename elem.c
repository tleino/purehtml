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

#include "elem.h"
#include "token.h"
#include "tagmap.h"
#include "attr.h"

#include <stdlib.h>
#include <assert.h>
#include <err.h>
#include <string.h>

struct elem *
elem_create(const char *name)
{
	struct	token token;

	assert(name != NULL);

	token = TOKEN_SET_START_TAG();
	token_set_tag_name(&token, name);

	return elem_create_from_token(&token);
}

const char *
elem_attr_value(struct elem *elem, const char *name)
{
	struct attr *attr;

	attr = attr_get(elem->attr, name);
	if (attr != NULL)
		return attr->value;
	return NULL;
}

void elem_set_attr(struct elem *elem, const char *name, const char *value)
{
	attr_set(&elem->attr, name, value);
}

int
elem_has_attr(struct elem *elem, const char *name)
{
	return attr_has(elem->attr, name);
}

struct elem *
elem_create_from_token(struct token *token)
{
	struct	elem *elem;

	assert(TOKEN_IS_START_END(token));

	elem = calloc(1, sizeof(struct elem));
	if (elem == NULL)
		err(1, "calloc elem");

	elem->tagid = token->u.tag.tagid;
	elem->name = token->u.tag.name;

	assert(elem->name != NULL);
	return elem;
}

size_t
elem_size(struct elem *elem)
{
	size_t sum;

	sum = 0;
	if (elem->tagid == TAG_CUSTOM_TAG && elem->name != NULL)
		sum += strlen(elem->name);

	sum += attr_size(elem->attr);
	return sum;
}

void
elem_free(struct elem *elem)
{
	struct attr	*attr, *next;

	assert(elem != NULL);
	assert(elem->name != NULL);

	if (elem->name != tagmap(elem->tagid)->name) {
		free(elem->name);
		elem->name = NULL;
	}

	attr = elem->attr;
	while (attr != NULL) {
		if (attr->name != NULL)
			free(attr->name);
		if (attr->value != NULL)
			free(attr->value);
		next = attr->next;
		free(attr);
		attr = next;
	}

	free(elem);
}
