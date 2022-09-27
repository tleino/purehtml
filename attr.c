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

#include "attr.h"

#include <stdlib.h>
#include <string.h>
#include <err.h>
#include <assert.h>
#include <strings.h>

#include "attrs.c"

#define ATTRMAP_SZ 1024

const struct attr_map *
attr_map_find(int i)
{
	assert(i >= 0 && i < ATTRMAP_SZ);
	return &attr_map[i];
}

int
attr_map_id(const char *s)
{
	static const int prime = 104729;
	static const int modulo = 1048576;
	size_t addr = 0;
	const char *p = s;

	while (*p != '\0') {
		addr += *p++;
		addr *= prime;
		addr %= modulo;
	}
	addr %= ATTRMAP_SZ;
	while (
	    attr_map[addr].name != NULL &&
	    strcmp(attr_map[addr].name, s) != 0) {
		addr++;
		addr %= ATTRMAP_SZ;
	}
	return addr;
}

struct attr *
attr_get(struct attr *head, const char *name)
{
	struct attr *attr;

	attr = head;
	while (attr != NULL) {
		if (attr->name != NULL && strcasecmp(attr->name, name) == 0)
			return attr;
		attr = attr->next;
	}

	return NULL;
}

size_t
attr_size(struct attr *attr)
{
	size_t sum = 0;

	while (attr != NULL) {
		if (attr->name != NULL)
			sum += strlen(attr->name);
		if (attr->value != NULL)
			sum += strlen(attr->value);

		attr = attr->next;
		sum += sizeof(struct attr);
	}

	return sum;
}

void
attr_set(struct attr **head, const char *name, const char *value)
{
	struct attr *attr;

	assert(name != NULL && *name != '\0');

	attr = attr_get(*head, name);
	if (attr == NULL) {
		attr = calloc(1, sizeof(struct attr));
		if (attr == NULL)
			err(1, "calloc attr");
		attr->next = (*head);
		*head = attr;
	}

	if (attr->name != NULL) {
		free(attr->name);
		attr->name = NULL;
	}
	attr->name = strdup(name);
	if (attr->name == NULL)
		err(1, "strdup attr name");

	if (attr->value != NULL) {
		free(attr->value);
		attr->value = NULL;
	}
	if (value != NULL) {
		attr->value = strdup(value);
		if (attr->value == NULL)
			err(1, "strdup attr value");
	}
}

int
attr_has(struct attr *head, const char *name)
{
	if (attr_get(head, name) == NULL)
		return 0;

	return 1;
}

#ifdef TEST
#include <stdio.h>
int main(int argc, char **argv)
{
	{
		int i;

		i = attr_map_id("onclick");
		assert(strcmp(attr_map_find(i)->name, "onclick") == 0);
	}

	{
		struct attr *head = NULL;

		attr_set(&head, "href", "foo");
		assert(strcmp(attr_get(head, "href")->value, "foo") == 0);
		assert(attr_has(head, "href") == 1);
		assert(attr_has(head, "src") == 0);
	}

	return 0;
}
#endif
