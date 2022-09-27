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

#include "cdata.h"
#include "util.h"

#include <stdlib.h>
#include <err.h>

struct cdata *
cdata_create(T_CDATA type)
{
	struct cdata *cdata;

	cdata = calloc(1, sizeof(struct cdata));
	if (cdata == NULL)
		err(1, "calloc cdata");
	cdata->type = type;

	return cdata;
}

void
cdata_add(struct cdata *cdata, const char *s)
{
	while (*s != '\0')
		str_add(&cdata->data, *s++);
}

size_t
cdata_size(struct cdata *cdata)
{
	return cdata->data.alloc;	
}

void
cdata_free(struct cdata *cdata)
{
	if (cdata->data.s != NULL)
		free(cdata->data.s);
	free(cdata);
}
