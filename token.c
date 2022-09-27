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

#include "token.h"
#include "attr.h"
#include "tagmap.h"

#include <stdlib.h>
#include <string.h>
#include <assert.h>
#include <err.h>

static void
tagtoken_set_attr(struct tagtoken *tagtoken, const char *name, const char *value);

const char*
token_str(struct token *token)
{
	static const char	*s[] = {
		"EMPTY", "CHAR", "DOCTYPE", "START_TAG", "END_TAG",
		"COMMENT"
	};

	assert(token != NULL);
	assert(token->type < sizeof(s) / sizeof(s[0]));
	return s[token->type];
}

void
token_set_tag_attr(struct token *token, const char *name, const char *value)
{
	assert(token != NULL);
	assert(TOKEN_IS_START_END(token));

	tagtoken_set_attr(&token->u.tag, name, value);
}

static void
tagtoken_set_attr(struct tagtoken *tagtoken, const char *name, const char *value)
{
	assert(tagtoken != NULL);

	attr_set(&tagtoken->attr, name, value);
}

void
token_clear(struct token *token)
{
	struct attr	*np;
	struct attr	*next;

	assert(token != NULL);

	if (!token->used && TOKEN_IS_START_END(token)) {
		if (token->u.tag.name != NULL &&
		    token->u.tag.name != tagmap(token->u.tag.tagid)->name)
			free(token->u.tag.name);

		np = token->u.tag.attr;
		while (np != NULL) {
			next = np->next;
			free(np);
			np = next;
		}
	} else if (TOKEN_IS_CHAR(token)) {
		str_add(&token->s, '\0');
	}

	if (TOKEN_IS_START_END(token))
		memset(&token->u.tag, '\0', sizeof(struct tagtoken));
	token->type = TOKEN_EMPTY;
	token->used = 0;
}

void
token_add_char(struct token *token, char c)
{
	str_add(&token->s, c);
}

void
token_set_tag_name(struct token *token, const char *name)
{
	assert(token != NULL);
	assert(name != NULL);

	token->u.tag.tagid = tagmap_id(name);
	if (token->u.tag.tagid == 0) {
		token->u.tag.name = strdup(name);
		if (token->u.tag.name == NULL)
			err(1, "strup token tag name");
	} else
		token->u.tag.name = tagmap(token->u.tag.tagid)->name;
}
