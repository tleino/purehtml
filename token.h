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

#ifndef TOKEN_H
#define TOKEN_H

#include "util.h"

struct attr;

typedef enum token_type {
	TOKEN_EMPTY,
	TOKEN_CHAR,
	TOKEN_DOCTYPE,
	TOKEN_START_TAG,
	TOKEN_END_TAG,
	TOKEN_COMMENT
} TOKEN_TYPE;

struct tagtoken
{
	unsigned short tagid;
	char *name;			/* can be ptr to tagid name */
	struct attr *attr;
	char is_self_closing;
};

struct str;

struct token
{
	TOKEN_TYPE type;
	union {
		struct tagtoken tag;
	} u;
	char used;			/* if used, alloc control is passed fwd */
	struct str s;
	size_t end_line;
};

const char		*token_str		(struct token *);
void			 token_set_tag_attr	(struct token *, const char *, const char *);
void			 token_clear		(struct token *);
void			 token_set_tag_name	(struct token *, const char *);
void			 token_add_char		(struct token *, char);

#define TOKEN_SET_DOCTYPE(_c) \
	((struct token) { .type = TOKEN_DOCTYPE })
#define TOKEN_SET_START_TAG() \
	((struct token) { .type = TOKEN_START_TAG })
#define TOKEN_SET_END_TAG() \
	((struct token) { .type = TOKEN_END_TAG })
#define TOKEN_SET_COMMENT() \
	((struct token) { .type = TOKEN_COMMENT })
#define TOKEN_IS_COMMENT(_token) \
	((_token)->type == TOKEN_COMMENT)
#define TOKEN_IS_CHAR(_token) \
	((_token)->type == TOKEN_CHAR)
#define TOKEN_IS_EMPTY(_token) \
	((_token)->type == TOKEN_EMPTY)

#define TOKEN_IS_SPACE(_token) \
	((_token)->type == TOKEN_CHAR && \
	HTML_ISSPACE((_token)->s.s[0]))

#define TOKEN_IS_DOCTYPE(_token) ((_token)->type == TOKEN_DOCTYPE)

#define TOKEN_IS_START(_token) ((_token)->type == TOKEN_START_TAG)
#define TOKEN_IS_END(_token) ((_token)->type == TOKEN_END_TAG)

#define TOKEN_IS_START_END(_token) \
	(TOKEN_IS_START(_token) || \
	TOKEN_IS_END(_token))

#define TOKEN_IS_START_TAG(_token, _tagid) \
	((_token)->type == TOKEN_START_TAG && \
	(_token)->u.tag.tagid == (_tagid))

#define TOKEN_IS_END_TAG(_token, _tagid) \
	((_token)->type == TOKEN_END_TAG && \
	(_token)->u.tag.tagid == (_tagid))

#define TOKEN_IS_TAG(_token, _tagid) ((_token)->u.tag.tagid == (_tagid))

#endif
