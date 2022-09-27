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

#include "tokenize.h"
#include "util.h"

#include <ctype.h>
#include <assert.h>
#include <err.h>
#include <string.h>

#include "states.c"

static void		 enter_state_return(struct tokenizer *, STATE, STATE);
static struct token	*enter_state_emit(struct tokenizer *, STATE, struct token *);
static struct token	*enter_state_emit_char(struct tokenizer *, STATE, char);
static struct token	*enter_state_emit_doctype(struct tokenizer *, STATE);
static void		 enter_state_reconsume(struct tokenizer *, STATE, char);
static void		 enter_state(struct tokenizer *, STATE);
static void		 print_err(struct tokenizer *, const char *);
static void		 enter_state_err(struct tokenizer *, STATE, const char *);
static void		 push_char(struct tokenizer *, char);

static int cnt;

struct token *
tokenize(struct tokenizer *ctx)
{
	char c;
	char *p;

	assert(ctx->fp != NULL);

	c = fgetc(ctx->fp);

	if (c == EOF)
		return NULL;

	if (c == '\n')
		ctx->line++;

	/*
	 * Additional prefilter for all modes and states, simplifying
	 * the state machines.
	 */
	if (iscntrl(c) && (c != '\n' && c != '\t'))
		return NULL;

	/*
	 * Whitespace rules.
	 */
	switch (ctx->state) {
	case STATE_BEFORE_ATTRIB_NAME:
	case STATE_AFTER_ATTRIB_NAME:
	case STATE_BEFORE_ATTRIB_VAL:
		if (HTML_ISSPACE(c))
			return NULL;
		break;
	default:
		break;
	}

	switch (ctx->state) {
	/*
	 *
	 */
	case STATE_SCRIPT_DATA:
		if (c == '<')
			enter_state(ctx, STATE_SCRIPT_DATA_LT);
		else
			return enter_state_emit_char(ctx, STATE_SCRIPT_DATA, c);
		break;
	case STATE_SCRIPT_DATA_LT:
		if (c == '/')
			enter_state(ctx, STATE_SCRIPT_DATA_END_TAG_OPEN);
		else if (c == '!')
			enter_state(ctx, STATE_SCRIPT_DATA_ESC_START);
		else
			enter_state_reconsume(ctx, STATE_SCRIPT_DATA, c);
		break;
	case STATE_SCRIPT_DATA_ESC_START:
		if (c == '-')
			enter_state(ctx, STATE_SCRIPT_DATA_ESC_START_DASH);
		else
			enter_state_reconsume(ctx, STATE_SCRIPT_DATA, c);
		break;
	case STATE_SCRIPT_DATA_ESC_START_DASH:
		if (c == '-')
			enter_state(ctx, STATE_SCRIPT_DATA_ESC_DASH2);
		else
			enter_state_reconsume(ctx, STATE_SCRIPT_DATA, c);
		break;
	case STATE_SCRIPT_DATA_ESC_DASH:
		if (c == '-')
			return enter_state_emit_char(ctx,
			    STATE_SCRIPT_DATA_ESC_DASH2, c);
		else if (c == '<')
			enter_state(ctx, STATE_SCRIPT_DATA_ESC_LT);
		else if (c == '>')
			enter_state_emit_char(ctx, STATE_SCRIPT_DATA, c);
		else
			enter_state_emit_char(ctx, STATE_SCRIPT_DATA_ESC, c);
		break;
	case STATE_SCRIPT_DATA_ESC_DASH2:
		if (c == '-')
			return enter_state_emit_char(ctx,
			    STATE_SCRIPT_DATA_ESC_DASH2, c);
		else if (c == '<')
			enter_state(ctx, STATE_SCRIPT_DATA_ESC_LT);
		else if (c == '>')
			enter_state_emit_char(ctx, STATE_SCRIPT_DATA, c);
		else
			enter_state_emit_char(ctx, STATE_SCRIPT_DATA_ESC, c);
		break;
	case STATE_SCRIPT_DATA_ESC_LT:
		if (c == '/')
			enter_state(ctx, STATE_SCRIPT_DATA_ESC_END_TAG_OPEN);
		else if (isalpha(c))
			enter_state(ctx, STATE_SCRIPT_DATA_DBL_ESC_START);
		else
			enter_state_reconsume(ctx, STATE_SCRIPT_DATA_ESC, c);
		break;
	case STATE_SCRIPT_DATA_ESC_END_TAG_OPEN:
		if (isalpha(c))
			enter_state_reconsume(ctx,
			    STATE_SCRIPT_DATA_ESC_END_TAG_NAME, c);
		else
			enter_state_reconsume(ctx, STATE_SCRIPT_DATA_ESC, c);
		break;
	case STATE_SCRIPT_DATA_ESC_END_TAG_NAME:
		if (HTML_ISSPACE(c))
			enter_state(ctx, STATE_BEFORE_ATTRIB_NAME);
		else
			enter_state_reconsume(ctx, STATE_SCRIPT_DATA_ESC, c);
		break;
	case STATE_SCRIPT_DATA_DBL_ESC_START:
		if (HTML_ISSPACE(c) || c == '/' || c == '>')
			enter_state(ctx, STATE_SCRIPT_DATA_ESC);
		else if (isalpha(c)) {
			c = tolower(c);
			return enter_state_emit(ctx,
			    STATE_SCRIPT_DATA_DBL_ESC_START, &ctx->token);
		} else
			enter_state_reconsume(ctx, STATE_SCRIPT_DATA_ESC, c);
		break;
	case STATE_SCRIPT_DATA_ESC:
		if (c == '-')
			enter_state(ctx, STATE_SCRIPT_DATA_ESC_DASH);
		else if (c == '<')
			enter_state(ctx, STATE_SCRIPT_DATA_ESC_LT);
		else
			return enter_state_emit_char(ctx, STATE_SCRIPT_DATA_ESC, c);
		break;
	case STATE_SCRIPT_DATA_END_TAG_OPEN:
		if (isalpha(c)) {
			ctx->token = TOKEN_SET_END_TAG();
			enter_state_reconsume(ctx, STATE_SCRIPT_DATA_END_TAG_NAME, c);
		} else
			enter_state_reconsume(ctx, STATE_SCRIPT_DATA, c);
		break;
	case STATE_SCRIPT_DATA_END_TAG_NAME:
		if (HTML_ISSPACE(c))
			enter_state(ctx, STATE_BEFORE_ATTRIB_NAME);
		else if (c == '>') {
			if (strcmp(ctx->name.s, "script") != 0) {
				return enter_state_emit_char(ctx, STATE_SCRIPT_DATA, '<');
			} else {
				token_set_tag_name(&ctx->token, ctx->name.s);
				return enter_state_emit(ctx, STATE_DATA, &ctx->token);
			}
		} else if (isalpha(c)) {
			str_add(&ctx->name, tolower(c));
		} else {
			return enter_state_emit_char(ctx, STATE_SCRIPT_DATA, '<');
		}
		break;

	/*
	 *
	 */
	case STATE_RAWTEXT:
		if (c == '<')
			enter_state(ctx, STATE_RAWTEXT_LT);
		else
			return enter_state_emit_char(ctx, STATE_RAWTEXT, c);
		break;
	case STATE_RAWTEXT_LT:
		if (c == '/')
			enter_state(ctx, STATE_RAWTEXT_END_TAG_OPEN);
		else {
			enter_state_emit_char(ctx, STATE_RAWTEXT, '<');
			enter_state_reconsume(ctx, STATE_RAWTEXT, c);
		}
		break;
	case STATE_RAWTEXT_END_TAG_OPEN:
		if (isalpha(c)) {
			ctx->token = TOKEN_SET_END_TAG();
			enter_state_reconsume(ctx, STATE_RAWTEXT_END_TAG_NAME,
			    c);
		} else {
			enter_state_emit_char(ctx, STATE_RAWTEXT, '<');
			enter_state_emit_char(ctx, STATE_RAWTEXT, '/');
			enter_state_reconsume(ctx, STATE_RAWTEXT, c);
		}
		break;
	case STATE_RAWTEXT_END_TAG_NAME:
		if (HTML_ISSPACE(c)) {
			enter_state(ctx, STATE_BEFORE_ATTRIB_NAME);
		} else if (c == '/') {
		} else if (c == '>') {
			token_set_tag_name(&ctx->token, ctx->name.s);
			return enter_state_emit(ctx, STATE_DATA, &ctx->token);
		} else if (isalpha(c)) {
			c = tolower(c);
			str_add(&ctx->name, tolower(c));
		} else
			enter_state_reconsume(ctx, STATE_RAWTEXT, c);
		break;

	/*
	 *
	 */
	case STATE_RCDATA:
		if (c == '&')
			enter_state_return(ctx, STATE_CHARACTER_REFERENCE,
			    STATE_RCDATA);
		else if (c == '<')
			enter_state(ctx, STATE_RCDATA_LT);
		else
			return enter_state_emit_char(ctx, STATE_RCDATA, c);
		break;
	case STATE_RCDATA_LT:
		if (c == '/')
			enter_state(ctx, STATE_RCDATA_END_TAG_OPEN);
		else {
			enter_state_reconsume(ctx, STATE_RCDATA, c);
			return enter_state_emit_char(ctx, STATE_RCDATA, '<');
		}
		break;
	case STATE_RCDATA_END_TAG_OPEN:
		if (isalpha(c)) {
			ctx->token = TOKEN_SET_END_TAG();
			enter_state_reconsume(ctx, STATE_RCDATA_END_TAG_NAME, c);
		} else {
			push_char(ctx, '<');
			push_char(ctx, '/');
			enter_state_reconsume(ctx, STATE_RCDATA, c);
			return &ctx->token;
		}
		break;
	case STATE_RCDATA_END_TAG_NAME:
		if (HTML_ISSPACE(c))
			enter_state(ctx, STATE_BEFORE_ATTRIB_NAME);
		else if (c == '/')
			ctx->token.u.tag.is_self_closing = 1;
		else if (c == '>') {
			token_set_tag_name(&ctx->token, ctx->name.s);
			return enter_state_emit(ctx, STATE_DATA, &ctx->token);
		} else if (isalpha(c)) {
			c = tolower(c);
			str_add(&ctx->name, c);
		} else {
			push_char(ctx, '<');
			push_char(ctx, '/');
			p = ctx->name.s;
			while (*p != '\0')
				push_char(ctx, *p++);
			enter_state_reconsume(ctx, STATE_RCDATA, c);
			return &ctx->token;
		}
		break;
	case STATE_DATA:
		switch (c) {
		case '<':
			enter_state(ctx, STATE_TAG_OPEN);
			return NULL;
		case '&':
			enter_state_return(ctx, STATE_CHARACTER_REFERENCE, STATE_DATA);
			return NULL;
		default:
			return enter_state_emit_char(ctx, STATE_DATA, c);
		}
		break;
	case STATE_TAG_OPEN:
		switch (c) {
		case '/':
			enter_state(ctx, STATE_END_TAG_OPEN);
			return NULL;
		case '!':
			enter_state(ctx, STATE_MARKUP_DECLARATION_OPEN);
			return NULL;
		default:
			if (isalpha(c)) {
				ctx->token = TOKEN_SET_START_TAG();
				enter_state_reconsume(ctx, STATE_TAG_NAME, c);
				return NULL;
			}
			break;
		}
		break;
	case STATE_MARKUP_DECLARATION_OPEN:
		ctx->buf[ctx->buf_len++] = c;
		ctx->buf[ctx->buf_len] = '\0';

		if (ctx->match == NULL) {
			switch (c) {
			case '-':
				ctx->match = "--";
				return NULL;
			case 'D':
				ctx->match = "DOCTYPE";
				return NULL;
			case '[':
				ctx->match = "[CDATA[";
				return NULL;
			default:
				goto err_markup_declaration_open;
			}
		} else if (ctx->buf[ctx->buf_len-1] != ctx->match[ctx->buf_len-1]) {
			goto err_markup_declaration_open;
		} else if (ctx->match[ctx->buf_len] == '\0') {
			switch (ctx->match[0]) {
			case '-':
				enter_state(ctx, STATE_COMMENT_START);
				return NULL;
			case 'D':
				enter_state(ctx, STATE_DOCTYPE);
				return NULL;
			case '[':
				enter_state_err(ctx, STATE_BOGUS_COMMENT,
				    "cdata-in-html-content");
				return NULL;
			default:
				assert(0);
			}
		} else
			return NULL;

err_markup_declaration_open:
		warnx("incorrectly-opened-comment");
		enter_state(ctx, STATE_BOGUS_COMMENT);
		return NULL;
	case STATE_DOCTYPE:
		if (HTML_ISSPACE(c)) {
			enter_state(ctx, STATE_BEFORE_DOCTYPE_NAME);
			return NULL;
		} else if (c == '>') {
			enter_state_reconsume(ctx, STATE_BEFORE_DOCTYPE_NAME, c);
			return NULL;
		} else {
			print_err(ctx,
			    "missing-whitespace-before-doctype-name");
			enter_state_reconsume(ctx, STATE_BEFORE_DOCTYPE_NAME, c);
			return NULL;
		}
		break;
	case STATE_BEFORE_DOCTYPE_NAME:
		if (HTML_ISSPACE(c)) {
			return NULL;
		} else if (c == '>') {
			enter_state_err(ctx, STATE_DATA,
			    "missing-doctype-name");
			return NULL;
		} else {
			enter_state(ctx, STATE_DOCTYPE_NAME);
			str_add(&ctx->name, '\0');
			str_add(&ctx->name, c);
			return NULL;
		}
		break;
	case STATE_DOCTYPE_NAME:
		if (HTML_ISSPACE(c)) {
			enter_state(ctx, STATE_AFTER_DOCTYPE_NAME);
			return NULL;
		} else if (c == '>') {
			enter_state_emit_doctype(ctx, STATE_DATA);
			return NULL;
		} else if (isupper(c)) {
			c = tolower(c);
			str_add(&ctx->name, c);
		} else {
			str_add(&ctx->name, c);
		}
		break;
	case STATE_AFTER_DOCTYPE_NAME:
		if (HTML_ISSPACE(c))
			return NULL;
		else if (c == '>') {
			enter_state_emit_doctype(ctx, STATE_DATA);
			return NULL;
		} else {
			enter_state(ctx, STATE_BOGUS_DOCTYPE);
			return NULL;
		}
		break;
	case STATE_BOGUS_DOCTYPE:
		switch (c) {
		case '>':
			enter_state_emit_doctype(ctx, STATE_DATA);
			return NULL;
		default:
			return NULL;
		}
		break;
	case STATE_END_TAG_OPEN:
		switch (c) {
		case '>':	/* parse error */
			enter_state(ctx, STATE_DATA);
			return NULL;
		}
		if (isalpha(c)) {
			ctx->token = TOKEN_SET_END_TAG();
			enter_state_reconsume(ctx, STATE_TAG_NAME, c);
			return NULL;
		} else {
			print_err(ctx, "invalid-first-character-of-tag-name");
			enter_state_reconsume(ctx, STATE_BOGUS_COMMENT, c);
			return NULL;
		}
		break;
	case STATE_TAG_NAME:
		if (HTML_ISSPACE(c)) {
			token_set_tag_name(&ctx->token, ctx->name.s);
			enter_state(ctx, STATE_BEFORE_ATTRIB_NAME);
			return NULL;
		}
		if (isupper(c))
			c = tolower(c);
			
		switch (c) {
		case '>':
			token_set_tag_name(&ctx->token, ctx->name.s);
			return enter_state_emit(ctx, STATE_DATA, &ctx->token);
		default:
			str_add(&ctx->name, c);
			return NULL;
		}
		break;
	case STATE_BEFORE_ATTRIB_NAME:
		if (c == '/' || c == '>')
			enter_state_reconsume(ctx, STATE_AFTER_ATTRIB_NAME, c);
		else if (c == '=') {
			enter_state_err(ctx, STATE_ATTRIB_NAME,
			    "unexpected-equals-sign-before-attribute-name");
		} else {
			enter_state_reconsume(ctx, STATE_ATTRIB_NAME, c);
		}
		break;
	case STATE_ATTRIB_NAME:
		if (HTML_ISSPACE(c) || c == '/' || c == '>')
			enter_state_reconsume(ctx, STATE_AFTER_ATTRIB_NAME, c);
		else if (c == '=')
			enter_state(ctx, STATE_BEFORE_ATTRIB_VAL);
		else {
			if (c == '\"' || c == '\'' || c == '<')
				print_err(ctx,
				    "unexpected-character-in-attribute-name");
			c = tolower(c);
			str_add(&ctx->attrib_name, c);
		}
		break;
	case STATE_AFTER_ATTRIB_NAME:
		token_set_tag_attr(&ctx->token,
		    ctx->attrib_name.s, ctx->attrib_value.s);
		if (c == '/') {
			enter_state(ctx, STATE_SELF_CLOSING_START_TAG);
		} else if (c == '=')
			enter_state(ctx, STATE_BEFORE_ATTRIB_VAL);
		else if (c == '>')
			return enter_state_emit(ctx, STATE_DATA, &ctx->token);
		else
			enter_state_reconsume(ctx, STATE_ATTRIB_NAME, c);
		break;
	case STATE_BEFORE_ATTRIB_VAL:
		if (c == '\"')
			enter_state(ctx, STATE_ATTRIB_VAL_QUOTED);
		else if (c == '\'')
			enter_state(ctx, STATE_ATTRIB_VAL_SQUOTED);
		else if (c == '>') {
			print_err(ctx, "missing-attribute-value");
			return enter_state_emit(ctx, STATE_DATA, &ctx->token);
		} else
			enter_state(ctx, STATE_ATTRIB_VAL);
		break;
	case STATE_ATTRIB_VAL_QUOTED:
		if (c == '\"')
			enter_state(ctx, STATE_AFTER_ATTRIB_VAL_QUOTED);
		else
			str_add(&ctx->attrib_value, c);
		break;
	case STATE_ATTRIB_VAL_SQUOTED:
		if (c == '\'')
			enter_state(ctx, STATE_AFTER_ATTRIB_VAL_QUOTED);
		else
			str_add(&ctx->attrib_value, c);
		break;
	case STATE_ATTRIB_VAL:
		if (HTML_ISSPACE(c)) {
			enter_state(ctx, STATE_BEFORE_ATTRIB_NAME);
		} else if (c == '&') {
			enter_state_return(ctx, STATE_CHARACTER_REFERENCE,
			    STATE_ATTRIB_VAL);
		} else if (c == '>') {
			token_set_tag_attr(&ctx->token,
			    ctx->attrib_name.s, ctx->attrib_value.s);
			return enter_state_emit(ctx, STATE_DATA, &ctx->token);
		} else if (c == '\"' || c == '\'' || c == '<' || c == '=' ||
		    c == '`') {
			print_err(ctx, "unexpected-character-in-unquoted-attribute-value");
		} else {
			str_add(&ctx->attrib_value, c);
			/* TODO */
		}
		break;
	case STATE_AFTER_ATTRIB_VAL_QUOTED:
		if (HTML_ISSPACE(c))
			enter_state(ctx, STATE_BEFORE_ATTRIB_NAME);
		else if (c == '/')
			enter_state(ctx, STATE_SELF_CLOSING_START_TAG);
		else if (c == '>') {
			token_set_tag_attr(&ctx->token,
			    ctx->attrib_name.s, ctx->attrib_value.s);
			return enter_state_emit(ctx, STATE_DATA, &ctx->token);
		} else {
			print_err(ctx, "missing-whitespace-between-attributes");
			enter_state_reconsume(ctx, STATE_BEFORE_ATTRIB_NAME, c);
		}
		break;
	case STATE_SELF_CLOSING_START_TAG:
		if (c == '>') {
			token_set_tag_attr(&ctx->token,
			    ctx->attrib_name.s, ctx->attrib_value.s);
			return enter_state_emit(ctx, STATE_DATA, &ctx->token);
		} else {
			print_err(ctx, "unexpected-solidus-in-tag");
			enter_state(ctx, STATE_BEFORE_ATTRIB_NAME);
		}
		break;
	case STATE_COMMENT_START:
		if (c == '-')
			enter_state(ctx, STATE_COMMENT_START_DASH);
		else if (c == '>') {
			print_err(ctx, "abrupt-closing-of-empty-comment");
			return enter_state_emit(ctx, STATE_DATA, &ctx->token);
		} else
			enter_state_reconsume(ctx, STATE_COMMENT, c);
		break;
	case STATE_COMMENT_START_DASH:
		if (c == '-')
			enter_state(ctx, STATE_COMMENT_END);
		else if (c == '>') {
			print_err(ctx, "abrupt-closing-of-empty-comment");
			enter_state(ctx, STATE_DATA);
		} else
			enter_state_reconsume(ctx, STATE_COMMENT, c);
		break;
	case STATE_COMMENT:
		if (c == '<')
			enter_state(ctx, STATE_COMMENT_LT);
		else if (c == '-')
			enter_state(ctx, STATE_COMMENT_END_DASH);
		break;
	case STATE_COMMENT_LT:
		if (c == '!')
			enter_state(ctx, STATE_COMMENT_LT_BANG);
		else if (c == '<') {
		} else
			enter_state_reconsume(ctx, STATE_COMMENT, c);
		break;
	case STATE_COMMENT_LT_BANG:
		if (c == '-')
			enter_state(ctx, STATE_COMMENT_LT_BANG_DASH);
		else
			enter_state_reconsume(ctx, STATE_COMMENT, c);
		break;
	case STATE_COMMENT_LT_BANG_DASH:
		if (c == '-')
			enter_state(ctx, STATE_COMMENT_LT_BANG_DASH2);
		else
			enter_state_reconsume(ctx, STATE_COMMENT_END_DASH, c);
		break;
	case STATE_COMMENT_LT_BANG_DASH2:
		if (c == '>')
			enter_state_reconsume(ctx, STATE_COMMENT_END, c);
		else {
			print_err(ctx, "nested-comment");
			enter_state_reconsume(ctx, STATE_COMMENT_END, c);
		}
		break;
	case STATE_COMMENT_END_DASH:
		if (c == '-')
			enter_state(ctx, STATE_COMMENT_END);
		else
			enter_state(ctx, STATE_COMMENT);
		break;
	case STATE_COMMENT_END:
		if (c == '>')
			enter_state(ctx, STATE_DATA);
		else if (c == '!')
			enter_state(ctx, STATE_COMMENT_END_BANG);
		else if (c == '-') {
		} else
			enter_state_reconsume(ctx, STATE_COMMENT, c);
		break;
	case STATE_COMMENT_END_BANG:
		if (c == '-')
			enter_state(ctx, STATE_COMMENT_END_DASH);
		else if (c == '>') {
			print_err(ctx, "incorrectly-closed-comment");
			enter_state(ctx, STATE_DATA);
		} else
			enter_state_reconsume(ctx, STATE_COMMENT, c);
		break;
	case STATE_BOGUS_COMMENT:
		if (c == '>')
			enter_state(ctx, STATE_DATA);
		break;
	case STATE_CHARACTER_REFERENCE:
		if (isalnum(c))
			enter_state_reconsume(ctx, STATE_NAMED_CHAR_REF, c);
		else if (c == '#')
			enter_state(ctx, STATE_NUM_CHAR_REF);
		else
			enter_state(ctx, ctx->return_state);
		break;
	case STATE_NAMED_CHAR_REF:
		enter_state(ctx, ctx->return_state);
		break;
	case STATE_NUM_CHAR_REF:
		enter_state(ctx, ctx->return_state);
		break;
	default:
		printf("State: %s\n", states[ctx->state]);
		print_err(ctx, "unhandled state");
		assert(0);
		break;
	}

	return NULL;
}

static void
enter_state(struct tokenizer *ctx, STATE state)
{
#if 0
	printf("State str: %-30s\n", states[state]);
#endif

	switch (state) {
	case STATE_SCRIPT_DATA_END_TAG_NAME:
	case STATE_RCDATA_END_TAG_NAME:
	case STATE_TAG_NAME:
		str_add(&ctx->name, '\0');
		break;
	case STATE_BEFORE_ATTRIB_VAL:
		str_add(&ctx->attrib_value, '\0');
		break;
	case STATE_BEFORE_ATTRIB_NAME:
		if (ctx->attrib_name.s != NULL &&
		    *ctx->attrib_name.s != '\0')
			token_set_tag_attr(&ctx->token,
			    ctx->attrib_name.s, ctx->attrib_value.s);
		break;
	case STATE_ATTRIB_NAME:
		str_add(&ctx->attrib_name, '\0');
		break;
	case STATE_MARKUP_DECLARATION_OPEN:
		ctx->match = NULL;
		ctx->buf_len = 0;
		break;
	case STATE_COMMENT_START:
	case STATE_BOGUS_COMMENT:
	case STATE_COMMENT:
		ctx->token = TOKEN_SET_COMMENT();
		break;
	default:
		break;
	}

	ctx->state = state;
}

static void
enter_state_return(struct tokenizer *ctx, STATE state, STATE ret)
{
	enter_state(ctx, state);
	ctx->return_state = ret;
}

static struct token *
enter_state_emit(struct tokenizer *ctx, STATE state, struct token *token)
{
	enter_state(ctx, state);
	ctx->token.end_line = ctx->line;

	return &ctx->token;
}

static void
push_char(struct tokenizer *ctx, char c)
{
	ctx->token.type = TOKEN_CHAR;
	ctx->token.end_line = ctx->line;
	str_add(&ctx->token.s, c);
}

static struct token *
enter_state_emit_char(struct tokenizer *ctx, STATE state, char c)
{
	push_char(ctx, c);
	enter_state(ctx, state);

	return &ctx->token;
}

static struct token *
enter_state_emit_doctype(struct tokenizer *ctx, STATE state)
{
	ctx->token = TOKEN_SET_DOCTYPE();
	ctx->token.end_line = ctx->line;

	enter_state(ctx, state);

	return &ctx->token;
}

static void
enter_state_reconsume(struct tokenizer *ctx, STATE state, char c)
{
	enter_state(ctx, state);

	cnt--;
#if 0
	printf("RECONSUME c='%c'\n", c);
#endif
	if (ungetc(c, ctx->fp) == EOF)
		err(1, "ungetc");
}

static void
print_err(struct tokenizer *ctx, const char *msg)
{
	assert(ctx != NULL);
	assert(msg != NULL);

	warnx("%zu: %s", ctx->line+1, msg);
}

static void
enter_state_err(struct tokenizer *ctx, STATE state, const char *msg)
{
	print_err(ctx, msg);
	enter_state(ctx, state);
}

#ifdef TEST
static void
dump_token(struct token *token)
{
	printf("%s ", token_str(token));
	if (token->type == TOKEN_START_TAG)
		printf("...tag %s (%d)\n", token->u.tag.name, token->u.tag.tagid);
	else if (token->type == TOKEN_END_TAG)
		printf("...tag end %s (%d)\n", token->u.tag.name, token->u.tag.tagid);
	else if (token->type == TOKEN_CHAR)
		printf("...char '%s'\n", token->s.s);
	else
		printf("\n");
}

int
main(int argc, char **argv)
{
	static struct tokenizer tokenizer;
	struct token *token;
	FILE *fp;
	static const char buf[] = \
	    "foobar<img src=foobar rel=\"zap\">zup</address>last";

	fp = fmemopen((void *) buf, sizeof(buf), "r");
	tokenizer.fp = fp;

	while (!feof(fp)) {
		token = tokenize(&tokenizer);
		if (token != NULL) {
			dump_token(token);
			token_clear(token);
		} else
			printf("NULL token\n");
	}
	fclose(fp);

	return 0;
}
#endif
