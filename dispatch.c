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

#include "dispatch.h"
#include "token.h"
#include "node.h"
#include "tagmap.h"
#include "util.h"
#include "ostack.h"
#include "cdata.h"
#include "elem.h"

#include <stdlib.h>
#include <assert.h>
#include <err.h>
#include <stdarg.h>

#include "imodes.c"
#include "states.h"

static void insert_char(struct dispatcher *, struct token *);
static void insert_close_tag(struct dispatcher *, struct token *);
static void insert_token_set_mode(struct dispatcher *, struct token *, IMODE);
static int insert_token_with_mode(struct dispatcher *, struct token *, IMODE);
static struct elem *pop();
static void print_err(struct dispatcher *, struct token *, IMODE, const char *);

/* helper */
int
dispatch(struct dispatcher *ctx, struct token *token,
    void (*begin)(struct node *), void(*end)(struct node *))
{
	ctx->begin = begin;
	ctx->end = end;
	return insert_token_with_mode(ctx, token, ctx->mode);
}

static void
insert_char(struct dispatcher *ctx, struct token *token)
{
	assert(token->type == TOKEN_CHAR);

	if (ctx->cdata == NULL)
		ctx->cdata = cdata_create(CDATA_TEXT);

	cdata_add(ctx->cdata, token->s.s);
	token->used = 1;
}

static struct elem *
insert_element_ns(struct dispatcher *ctx, struct token *token, int ns)
{
	struct elem *elem;
	struct node *node;

	assert(token->type == TOKEN_START_TAG);

	if (ctx->cdata != NULL) {
		ctx->begin(node_create_from_cdata(ctx->cdata));
		ctx->cdata = NULL;
	}

	elem = elem_create_from_token(token);
	elem->attr = token->u.tag.attr;

	if (elem->tagid)
		ctx->head_elem = elem;

	node = node_create_from_elem(elem);
	ctx->begin(node);

	if (!(tagmap(elem->tagid)->flags & TAG_EMPTY)) {
		ostack_push(elem);
	} else
		ctx->end(node);

	token->used = 1;

	return elem;
}

static struct elem *
insert_foreign_element(struct dispatcher *ctx, struct token *token, int ns)
{
	struct elem *elem;

	elem = insert_element_ns(ctx, token, ns);
	return elem;
}

static void
insert_tag(struct dispatcher *ctx, struct token *token)
{
	insert_element_ns(ctx, token, NS_HTML);
}

/* helper */
static void
insert_tag_name(struct dispatcher *ctx, const char *name, int close)
{
	struct token token;

	if (close)
		token = TOKEN_SET_END_TAG();
	else
		token = TOKEN_SET_START_TAG();

	token_set_tag_name(&token, name);

	if (close)
		insert_close_tag(ctx, &token);
	else
		insert_tag(ctx, &token);

	token_clear(&token);
}

/* helper */
static void
insert_tag_set_mode(struct dispatcher *ctx, struct token *token, IMODE mode)
{
	insert_tag(ctx, token);
	ctx->mode = mode;
}

/* helper */
static void
insert_tag_name_set_mode(struct dispatcher *ctx, const char *name, int close,
    IMODE mode)
{
	insert_tag_name(ctx, name, close);
	ctx->mode = mode;
}

static void
close_tag(struct dispatcher *ctx, struct elem *elem)
{
	if (ctx->cdata != NULL) {
		ctx->end(node_create_from_cdata(ctx->cdata));
		ctx->cdata = NULL;
	}

	ostack_pop();
	assert(elem->node != NULL);
	ctx->end(elem->node);
}

static void
insert_close_tag(struct dispatcher *ctx, struct token *token)
{
	struct elem *elem;

	assert(token->type == TOKEN_END_TAG);

	elem = ostack_peek();
	close_tag(ctx, elem);

	token->used = 0;
}

/* helper */
static void
insert_close_tag_set_mode(struct dispatcher *ctx, struct token *token,
    IMODE mode)
{
	insert_close_tag(ctx, token);
	ctx->mode = mode;
}

static int
is_open(int num_args, ...)
{
	size_t sz, i, j;
	va_list ap;

	sz = ostack_depth();

	for (i = sz; i >= 1; i--)  {
		va_start(ap, num_args);
		for (j = 0; j < num_args; j++)
			if (ostack_peek_at(i)->tagid == va_arg(ap, int))
				return 1;
		va_end(ap);
	}

	return 0;
}

static int
is_open_other_than(int num_args, ...)
{
	size_t sz, i, j;
	va_list ap;
	int found;

	sz = ostack_depth();
	found = 0;
	for (i = sz; i >= 1; i--)  {
		va_start(ap, num_args);
		found = 0;
		for (j = 0; j < num_args; j++) {
			if (ostack_peek_at(i)->tagid == va_arg(ap, int)) {
				found++;
			}
		}
		if (found == 0)
			return ostack_peek_at(i)->tagid;
		va_end(ap);
	}

	return -1;
}

enum scope {
	SCOPE_ANY,
	SCOPE_LIST_ITEM,
	SCOPE_BUTTON,
	SCOPE_TABLE,
	SCOPE_SELECT,
};

#if 0
static int
generate_implied_end_tags_thoroughly(struct dispatcher *ctx)
{
	size_t sz, i;

	sz = ostack_depth();
	for (i = sz; i >= 1; i--) {
		switch (ostack_peek_at(i)->tagid) {
		case TAG_CAPTION:
		case TAG_COLGROUP:
		case TAG_DD:
		case TAG_DT:
		case TAG_LI:
		case TAG_OPTGROUP:
		case TAG_OPTION:
		case TAG_P:
		case TAG_RB:
		case TAG_RP:
		case TAG_RT:
		case TAG_RTC:
		case TAG_TBODY:
		case TAG_TD:
		case TAG_TFOOT:
		case TAG_TH:
		case TAG_THEAD:
		case TAG_TR:
			pop(ctx);
		}
	}

	return 0;
}
#endif

static int
generate_implied_end_tags(struct dispatcher *ctx, int except)
{
	size_t sz, i;

	sz = ostack_depth();
	for (i = sz; i >= 1; i--) {
		if (ostack_peek_at(i)->tagid == except)
			return 0;

		switch (ostack_peek_at(i)->tagid) {
		case TAG_DD:
		case TAG_DT:
		case TAG_LI:
		case TAG_OPTGROUP:
		case TAG_OPTION:
		case TAG_P:
		case TAG_RB:
		case TAG_RP:
		case TAG_RT:
		case TAG_RTC:
			pop(ctx);
			break;
		default:
			return 0;
		}
	}
	return 0;
}

static int
close_p_element(struct dispatcher *ctx)
{
	generate_implied_end_tags(ctx, TAG_P);
	if (ostack_peek()->tagid != TAG_P)
		return 0;
	pop(ctx);
	return 1;
}

static int
has_element_in_scope(int target, int scope)
{
	size_t sz, i;

	sz = ostack_depth();
	for (i = sz; i >= 1; i--) {
		if (ostack_peek_at(i)->tagid == target)
			return 1;

		/*
		 * Terminate search if we reach...
		 */
		switch (scope) {
		case SCOPE_LIST_ITEM:
			switch (ostack_peek_at(i)->tagid) {
			case TAG_OL:
				return 0;
			case TAG_UL:
				return 0;
			}
			break;
		case SCOPE_BUTTON:
			switch (ostack_peek_at(i)->tagid) {
			case TAG_BUTTON:
				return 0;
			}
			break;
		case SCOPE_TABLE:
			switch (ostack_peek_at(i)->tagid) {
			case TAG_HTML:
			case TAG_TABLE:
			case TAG_TEMPLATE:
				return 0;
			}
			break;
		default:
			break;
		}

		if (scope == SCOPE_SELECT) {
			if (ostack_peek_at(i)->tagid == TAG_OPTGROUP)
				continue;
			if (ostack_peek_at(i)->tagid == TAG_OPTION)
				continue;
			return 0;
		}

		switch (scope) {
		case SCOPE_LIST_ITEM:
		case SCOPE_BUTTON:
		case SCOPE_ANY:
			switch (ostack_peek_at(i)->tagid) {
			case TAG_APPLET:
			case TAG_CAPTION:
			case TAG_HTML:
			case TAG_TABLE:
			case TAG_TD:
			case TAG_TH:
			case TAG_MARQUEE:
			case TAG_OBJECT:
			case TAG_TEMPLATE:
				return 0;
			}
		default:
			break;
		}
	}

	return 0;
}

static struct elem *
pop(struct dispatcher *ctx)
{
	struct elem *elem;

	elem = ostack_peek();
	close_tag(ctx, elem);
	return elem;
}

static struct elem *
pop_elem(struct dispatcher *ctx, int tagid)
{
	while (ostack_peek() != NULL) {
		if (ostack_peek()->tagid == tagid) {
			return pop(ctx);
		} else
			pop(ctx);
	}
	assert(0);
	return NULL;
}

static int
is_start_tag(struct token *token, int num_args, ...)
{
	va_list ap;
	size_t i;

	if (!TOKEN_IS_START(token))
		return 0;

	va_start(ap, num_args);
	for (i = 0; i < num_args; i++) {
		if (TOKEN_IS_START_TAG(token, va_arg(ap, int)))
			return 1;
	}

	return 0;
}


static int
is_end_tag(struct token *token, int num_args, ...)
{
	va_list ap;
	size_t i;

	if (!TOKEN_IS_END(token))
		return 0;

	va_start(ap, num_args);
	for (i = 0; i < num_args; i++) {
		if (TOKEN_IS_END_TAG(token, va_arg(ap, int)))
			return 1;
	}

	return 0;
}

static int
check_p(struct dispatcher *ctx, struct token *token, IMODE mode)
{
	if (has_element_in_scope(TAG_P, SCOPE_BUTTON)) {
		if (close_p_element(ctx) == 0) {
			print_err(ctx, token, mode, "closing p failed");
			return -1;
		}
	}
	return 0;
}

static void
adoption_agency(struct dispatcher *ctx, struct token *token)
{
}

static void
print_err(struct dispatcher *ctx, struct token *token, IMODE mode,
    const char *msg)
{
#define ERR_STR "%zu: parsing %s in %s"
	if (TOKEN_IS_START_END(token))
		warnx(ERR_STR "(%s): %s",
		    token->end_line+1,
		    token_str(token), imodes[mode], token->u.tag.name, msg);
	else if (TOKEN_IS_CHAR(token))
		warnx(ERR_STR "('%s'): %s",
		    token->end_line+1,
		    token_str(token), imodes[mode], token->s.s, msg);
	else
		warnx(ERR_STR ": %s",
		    token->end_line+1, token_str(token), imodes[mode], msg);
}

static IMODE
reset_imode(struct elem *head_elem_ptr)
{
	int last;
	size_t depth;
	struct elem *node;

	last = 0;
	depth = ostack_depth();
	node = ostack_peek_at(depth);

	if (node == NULL)
		return IMODE_INITIAL;

	do {
		if (depth == 1)
			last = 1;

		switch (node->tagid) {
		case TAG_TD:
		case TAG_TH:
			if (!last)
				return IMODE_IN_CELL;
			break;
		case TAG_TR:
			return IMODE_IN_ROW;
		case TAG_TBODY:
		case TAG_THEAD:
		case TAG_TFOOT:
			return IMODE_IN_TABLE_BODY;
		case TAG_CAPTION:
			return IMODE_IN_CAPTION;
		case TAG_TABLE:
			return IMODE_IN_TABLE;
		case TAG_TEMPLATE:
			return IMODE_IN_TEMPLATE;
		case TAG_HEAD:
			return IMODE_IN_HEAD;
		case TAG_BODY:
			return IMODE_IN_BODY;
		case TAG_FRAMESET:
			return IMODE_IN_FRAMESET;
		case TAG_HTML:
			if (head_elem_ptr == NULL)
				return IMODE_BEFORE_HEAD;
			return IMODE_AFTER_HEAD;
		}

		if (last)
			return IMODE_IN_BODY;

		node = ostack_peek_at(--depth);
		assert(node != NULL);
	} while(1);
}

enum context {
	CONTEXT_TABLE,
	CONTEXT_TABLE_BODY,
	CONTEXT_TABLE_ROW,
};
static void
clear_to_context(struct dispatcher *ctx, enum context c)
{
	int tagid;

	while (ostack_depth() >= 1) {
		tagid = ostack_peek()->tagid;
		if (c == CONTEXT_TABLE && (tagid == TAG_TABLE ||
		    tagid == TAG_TEMPLATE || tagid == TAG_HTML))
			return;
		if (c == CONTEXT_TABLE_BODY && (tagid == TAG_TBODY ||
		    tagid == TAG_TFOOT || tagid == TAG_THEAD ||
		    tagid == TAG_TEMPLATE || tagid == TAG_HTML))
			return;
		if (c == CONTEXT_TABLE_ROW && (tagid == TAG_TR ||
		    tagid == TAG_TEMPLATE || tagid == TAG_HTML))
			return;

		pop(ctx);
	}
	assert(0);
}

static void
close_cell(struct dispatcher *ctx, struct token *token)
{
	generate_implied_end_tags(ctx, -1);
	if (ostack_peek()->tagid == TAG_TD) {
		pop_elem(ctx, TAG_TD);
		ctx->mode = IMODE_IN_ROW;
	} else if (ostack_peek()->tagid == TAG_TH) {
		pop_elem(ctx, TAG_TH);
		ctx->mode = IMODE_IN_ROW;
	} else
		print_err(ctx, token, ctx->mode, "close cell");
}

/*
 * TODO: Add comment handling (now all comments are ignored altogether).
 * TODO: Add DOCTYPE token handling (now all are DOCTYPEs are ignored altogether).
 */
#include <stdio.h>
static int
insert_token_with_mode(struct dispatcher *ctx, struct token *token, IMODE mode)
{
	struct elem *node;
	int tagid;
	size_t sz;

#if 0
	printf("Insert %s (%s) ", token_str(token), imodes[mode]);
	if (token->type == TOKEN_START_TAG)
		printf("...tag %s (%d)\n", token->u.tag.name, token->u.tag.tagid);
	if (token->type == TOKEN_END_TAG)
		printf("...tag end %s (%d)\n", token->u.tag.name, token->u.tag.tagid);
	if (token->type == TOKEN_CHAR)
		printf("...char '%s'\n", token->s.s);
	printf("\n");
#endif

	if (TOKEN_IS_EMPTY(token) || TOKEN_IS_COMMENT(token))
		return STATE_NONE;

	/*
	 * Catch-all character insert.
	 */
	if (TOKEN_IS_CHAR(token)) {
		switch (mode) {
		case IMODE_IN_HEAD:
			if (ostack_peek() != NULL &&
			    ostack_peek()->tagid == TAG_TITLE) {
				insert_char(ctx, token);
				return STATE_NONE;
			}
			break;
#if 1
	/*
	 * Remembering script content is disabled.
	 */
		case IMODE_TEXT:
			return STATE_NONE;
#endif
		case IMODE_IN_BODY:
			insert_char(ctx, token);
			return STATE_NONE;
		default:
			break;
		}
	}

	if (mode != IMODE_INITIAL && TOKEN_IS_DOCTYPE(token)) {
		warnx("parse error (doctype not expected)");
		return STATE_NONE;
	}

	/*
	 * Whitespace rules.
	 */
	switch (mode) {
	case IMODE_INITIAL:
	case IMODE_BEFORE_HTML:
	case IMODE_BEFORE_HEAD:
		if (TOKEN_IS_SPACE(token))
			return STATE_NONE;
		break;
	case IMODE_IN_HEAD:
	case IMODE_IN_BODY:
	case IMODE_AFTER_HEAD:
		if (TOKEN_IS_SPACE(token)) {
			insert_char(ctx, token);
			return STATE_NONE;
		}
		break;
	default:
		break;
	}

	switch (mode) {
	case IMODE_INITIAL:
		if (TOKEN_IS_DOCTYPE(token)) {
			ctx->mode = IMODE_BEFORE_HTML;
			return STATE_NONE;
		} else
			insert_token_set_mode(ctx, token, IMODE_BEFORE_HTML);
		return STATE_NONE;
	case IMODE_BEFORE_HTML:
		if (TOKEN_IS_START_TAG(token, TAG_HTML)) {
			insert_tag_set_mode(ctx, token, IMODE_BEFORE_HEAD);
		} else {
			insert_tag_name_set_mode(ctx, "html", 0, IMODE_BEFORE_HEAD);
			return dispatch(ctx, token, ctx->begin, ctx->end);
		}
		return STATE_NONE;
	case IMODE_BEFORE_HEAD:
		if (TOKEN_IS_START_TAG(token, TAG_HEAD)) {
			insert_tag_set_mode(ctx, token, IMODE_IN_HEAD);
		} else {
			insert_tag_name_set_mode(ctx, "head", 0, IMODE_IN_HEAD);
			insert_token_set_mode(ctx, token, IMODE_IN_HEAD);
		}
		return STATE_NONE;
	case IMODE_IN_HEAD:
		if (TOKEN_IS_START_TAG(token, TAG_TITLE)) {
			insert_tag(ctx, token);
			return STATE_RCDATA;
		} else if (TOKEN_IS_END_TAG(token, TAG_HEAD)) {
			insert_close_tag_set_mode(ctx, token, IMODE_AFTER_HEAD);
			return STATE_NONE;
		} else if (is_start_tag(token, 5, TAG_META, TAG_BASE,
		    TAG_BASEFONT, TAG_BGSOUND, TAG_LINK)) {
			insert_tag(ctx, token);
			return STATE_NONE;
		} else if (TOKEN_IS_END_TAG(token, TAG_TITLE)) {
			/*
			 * TODO: Report this to HTML LS spec, expecting
			 * title end tag is not specified
			 */
			insert_close_tag(ctx, token);
			return STATE_NONE;
		} else if (is_start_tag(token, 2, TAG_NOFRAMES, TAG_STYLE)) {
			insert_tag(ctx, token);
			ctx->orig_mode = ctx->mode;
			ctx->mode = IMODE_TEXT;
			return STATE_RAWTEXT;
		} else if (TOKEN_IS_START_TAG(token, TAG_NOSCRIPT)) {
			insert_tag_set_mode(ctx, token, IMODE_IN_HEAD_NOSCRIPT);
			print_err(ctx, token, mode, "in head noscript");
			return STATE_NONE;
		} else if (TOKEN_IS_START_TAG(token, TAG_SCRIPT)) {
			ctx->orig_mode = ctx->mode;
			ctx->mode = IMODE_TEXT;
			insert_tag(ctx, token);
			return STATE_SCRIPT_DATA;			
		} else {
		/* TODO: What should we do here if we already popped
		 *       head off? */
			pop_elem(ctx, TAG_HEAD);
			print_err(ctx, token, mode, "force head");
			insert_token_set_mode(ctx, token, IMODE_AFTER_HEAD);
			return STATE_NONE;
		}
		break;
	case IMODE_TEXT:
		if (TOKEN_IS_END_TAG(token, TAG_SCRIPT)) {
			insert_close_tag_set_mode(ctx, token, ctx->orig_mode);
			return STATE_NONE;
		} else {
			pop(ctx);
			ctx->mode = ctx->orig_mode;
			return STATE_NONE;
		}
		break;
	case IMODE_AFTER_HEAD:
		if (TOKEN_IS_START_TAG(token, TAG_HTML))
			insert_token_with_mode(ctx, token, IMODE_IN_BODY);
		else if (TOKEN_IS_START_TAG(token, TAG_BODY))
			insert_tag_set_mode(ctx, token, IMODE_IN_BODY);
		else {
			insert_tag_name_set_mode(ctx, "body", 0, IMODE_IN_BODY);
			return dispatch(ctx, token, ctx->begin, ctx->end);
		}
		return STATE_NONE;
	case IMODE_IN_SELECT:
		if (TOKEN_IS_END_TAG(token, TAG_SELECT)) {
			if (!has_element_in_scope(TAG_SELECT,
			    SCOPE_SELECT)) {
				print_err(ctx, token, mode, "no select tag");
				return STATE_NONE;
			}
			pop_elem(ctx, TAG_SELECT);
			ctx->mode = reset_imode(ctx->head_elem);
			return STATE_NONE;
		}
		if (TOKEN_IS_START_TAG(token, TAG_OPTION)) {
			if (ostack_peek()->tagid == TAG_OPTION)
				pop(ctx);
			insert_tag(ctx, token);
			return STATE_NONE;
		}
		if (TOKEN_IS_START_TAG(token, TAG_OPTGROUP)) {
			if (ostack_peek()->tagid == TAG_OPTION)
				pop(ctx);
			if (ostack_peek()->tagid == TAG_OPTGROUP)
				pop(ctx);
			insert_tag(ctx, token);
			return STATE_NONE;
		}
		return STATE_NONE;
	case IMODE_IN_BODY:
		if (TOKEN_IS_COMMENT(token) ||
		    TOKEN_IS_DOCTYPE(token)) {
			print_err(ctx, token, mode, "did not expect");
			return STATE_NONE;
		}
		if (TOKEN_IS_START_TAG(token, TAG_HTML) ||
		    TOKEN_IS_START_TAG(token, TAG_BODY)) {
			print_err(ctx, token, mode, "did not expect");
			return STATE_NONE;
		}
		if (TOKEN_IS_START_TAG(token, TAG_FRAMESET)) {
			print_err(ctx, token, mode, "did not expect");
			return STATE_NONE;
		}
		if (TOKEN_IS_START_TAG(token, TAG_SELECT)) {
			/*
			 * We might be here temporarily even though our
			 * mode is different than 'in body'.
			 */
			if (ctx->mode == IMODE_IN_TABLE ||
			    ctx->mode == IMODE_IN_CAPTION ||
			    ctx->mode == IMODE_IN_TABLE_BODY ||
			    ctx->mode == IMODE_IN_ROW ||
			    ctx->mode == IMODE_IN_CELL)
				insert_tag_set_mode(ctx, token,
				    IMODE_IN_SELECT_IN_TABLE);
			else
				insert_tag_set_mode(ctx, token,
				    IMODE_IN_SELECT);
			return STATE_NONE;
		}
		if (TOKEN_IS_START_TAG(token, TAG_BASE) ||
		    TOKEN_IS_START_TAG(token, TAG_BASEFONT) ||
		    TOKEN_IS_START_TAG(token, TAG_BGSOUND) ||
		    TOKEN_IS_START_TAG(token, TAG_LINK) ||
		    TOKEN_IS_START_TAG(token, TAG_META) ||
		    TOKEN_IS_START_TAG(token, TAG_NOFRAMES) ||
		    TOKEN_IS_START_TAG(token, TAG_SCRIPT) ||
		    TOKEN_IS_START_TAG(token, TAG_STYLE) ||
		    TOKEN_IS_START_TAG(token, TAG_TEMPLATE) ||
		    TOKEN_IS_START_TAG(token, TAG_TITLE) ||
		    TOKEN_IS_END_TAG(token, TAG_TEMPLATE)) {
			return insert_token_with_mode(ctx, token, IMODE_IN_HEAD);
		}
		if (TOKEN_IS_END_TAG(token, TAG_BODY) ||
		    TOKEN_IS_END_TAG(token, TAG_HTML)) {
			if (!is_open(1, TAG_BODY)) {
				print_err(ctx, token, mode, "body was not open");
				return STATE_NONE;
			}
			tagid = is_open_other_than(18, TAG_DD, TAG_DT, TAG_LI,
			    TAG_OPTGROUP, TAG_OPTION, TAG_P, TAG_RB, TAG_RP,
			    TAG_RT, TAG_RTC, TAG_TBODY, TAG_TD, TAG_TFOOT,
			    TAG_TH, TAG_THEAD, TAG_TR, TAG_BODY, TAG_HTML);
			if (tagid != -1) {
				warnx("STILL OPEN: %s", tagmap(tagid)->name);
				print_err(ctx, token, mode,
				    "elem is still open");
				return STATE_NONE;
			}
			if (TOKEN_IS_END_TAG(token, TAG_BODY)) {
				insert_close_tag(ctx, token);
				return STATE_NONE;
			}
			return insert_token_with_mode(ctx, token, IMODE_AFTER_BODY);
		}
		if (is_start_tag(token, 24, TAG_ADDRESS, TAG_ARTICLE,
		    TAG_ASIDE, TAG_BLOCKQUOTE, TAG_CENTER, TAG_DETAILS,
		    TAG_DIALOG, TAG_DIR, TAG_DIV, TAG_DL, TAG_FIELDSET,
		    TAG_FIGCAPTION, TAG_FIGURE, TAG_FOOTER, TAG_HEADER,
		    TAG_HGROUP, TAG_MAIN, TAG_MENU, TAG_NAV, TAG_OL,
		    TAG_P, TAG_SECTION, TAG_SUMMARY, TAG_UL)) {
			if (check_p(ctx, token, mode) == -1)
				return STATE_NONE;
			insert_tag(ctx, token);
			return STATE_NONE;
		}
		if (TOKEN_IS_END(token) &&
		    tagmap(token->u.tag.tagid)->flags & TAG_HEADING) {
			if (!has_element_in_scope(token->u.tag.tagid,
			    SCOPE_ANY)) {
				print_err(ctx, token, mode, "no heading tag");
				return STATE_NONE;
			}
			generate_implied_end_tags(ctx, token->u.tag.tagid);
			if (ostack_peek()->tagid != token->u.tag.tagid) {
				print_err(ctx, token, mode, "did not match");
				return STATE_NONE;
			}
			pop_elem(ctx, token->u.tag.tagid);
			return STATE_NONE;
		}
		if (TOKEN_IS_START_TAG(token, TAG_MATH)) {
			insert_foreign_element(ctx, token, NS_MATHML);
			return STATE_NONE;
		}
		if (TOKEN_IS_START_TAG(token, TAG_SVG)) {
			insert_foreign_element(ctx, token, NS_SVG);
			return STATE_NONE;
		}
		if (TOKEN_IS_START_TAG(token, TAG_A)) {
			insert_tag(ctx, token);
			return STATE_NONE;
		}
		if (TOKEN_IS_START(token) &&
		    tagmap(token->u.tag.tagid)->flags & TAG_FORMAT) {
			insert_tag(ctx, token);
			return STATE_NONE;
		}
		if (TOKEN_IS_START_TAG(token, TAG_TABLE)) {
			if (check_p(ctx, token, mode) == -1)
				return STATE_NONE;
			insert_tag_set_mode(ctx, token, IMODE_IN_TABLE);	
			return STATE_NONE;
		}
		if (TOKEN_IS_START_TAG(token, TAG_NOBR)) {
			/* TODO adoption agency etc */
			print_err(ctx, token, mode, "TODO nobr");
			return STATE_NONE;
		}
		if (TOKEN_IS_END(token) &&
		    (tagmap(token->u.tag.tagid)->flags & TAG_FORMAT ||
		    token->u.tag.tagid == TAG_A ||
		    token->u.tag.tagid == TAG_NOBR)) {
			adoption_agency(ctx, token);
			insert_close_tag(ctx, token);
			return STATE_NONE;
		}
		if (is_start_tag(token, 3, TAG_APPLET, TAG_MARQUEE, TAG_OBJECT)) {
			print_err(ctx, token, mode, "TODO applet, marquee, object");
			return STATE_NONE;
		}
		if (TOKEN_IS_END_TAG(token, TAG_BR)) {
			print_err(ctx, token, mode, "TODO end for br");
			return STATE_NONE;
		}
		if (is_start_tag(token, 6, TAG_AREA, TAG_BR, TAG_EMBED, TAG_IMG,
		    TAG_KEYGEN, TAG_WBR)) {
			insert_tag(ctx, token);
			return STATE_NONE;
		}
		if (TOKEN_IS_START(token) &&
		    tagmap(token->u.tag.tagid)->flags & TAG_HEADING) {
			if (check_p(ctx, token, mode) == -1)
				return STATE_NONE;
			if (tagmap(ostack_peek()->tagid)->flags & TAG_HEADING) {
				print_err(ctx, token, mode, "was not H tag");
				pop(ctx);
			}
			insert_tag(ctx, token);
			return STATE_NONE;
		}
		if (is_start_tag(token, 2, TAG_PRE, TAG_LISTING)) {
			if (check_p(ctx, token, mode) == -1)
				return STATE_NONE;
			/* TODO: Ignore "Newlines at the start of pre blocks" */
			insert_tag(ctx, token);
			return STATE_NONE;
		}
		if (TOKEN_IS_START_TAG(token, TAG_FORM)) {
			/* TODO */
			if (check_p(ctx, token, mode) == -1)
				return STATE_NONE;
			insert_tag(ctx, token);
			return STATE_NONE;
		}
		if (TOKEN_IS_START_TAG(token, TAG_BUTTON)) {
			if (has_element_in_scope(TAG_BUTTON, SCOPE_ANY)) {
				print_err(ctx, token, mode, "already button");
				generate_implied_end_tags(ctx, -1);
				pop_elem(ctx, TAG_BUTTON);
			}
			insert_tag(ctx, token);
			return STATE_NONE;
		}
		if (is_end_tag(token, 26, TAG_ADDRESS, TAG_ARTICLE, TAG_ASIDE,
		    TAG_BLOCKQUOTE, TAG_BUTTON, TAG_CENTER, TAG_DETAILS,
		    TAG_DIALOG, TAG_DIR, TAG_DIV, TAG_DL, TAG_FIELDSET,
		    TAG_FIGCAPTION, TAG_FIGURE, TAG_FOOTER, TAG_HEADER,
		    TAG_HGROUP, TAG_LISTING, TAG_MAIN, TAG_MENU, TAG_NAV,
		    TAG_OL, TAG_PRE, TAG_SECTION, TAG_SUMMARY, TAG_UL)) {
			if (!has_element_in_scope(token->u.tag.tagid,
			    SCOPE_ANY)) {
				print_err(ctx, token, mode, "did not match");
				return STATE_NONE;
			}
			generate_implied_end_tags(ctx, 0);
			if (ostack_peek()->ns != NS_HTML ||
			    ostack_peek()->tagid != token->u.tag.tagid) {
				print_err(ctx, token, mode, "did not match");
				return STATE_NONE;
			}
			pop_elem(ctx, token->u.tag.tagid);
			return STATE_NONE;
		}
		if (is_end_tag(token, 2, TAG_DD, TAG_DT)) {
			if (!has_element_in_scope(token->u.tag.tagid,
			    SCOPE_ANY)) {
				print_err(ctx, token, mode, "no dd/dt tag");
				return STATE_NONE;
			}
			generate_implied_end_tags(ctx, token->u.tag.tagid);
			if (ostack_peek()->tagid != token->u.tag.tagid) {
				print_err(ctx, token, mode, "did not match");
				return STATE_NONE;
			}
			pop_elem(ctx, token->u.tag.tagid);
			return STATE_NONE;
		}
		if (is_start_tag(token, 2, TAG_DD, TAG_DT)) {
			sz = ostack_depth();
dd_dt_loop:
			tagid = ostack_peek_at(sz)->tagid;
			if (tagid == TAG_DD || tagid == TAG_DT) {
				generate_implied_end_tags(ctx, tagid);
				if (ostack_peek()->tagid != tagid) {
					print_err(ctx, token, mode,
					    "did not match");
					return STATE_NONE;
				}
				pop(ctx);
				goto dd_dt_done;
			} else if (tagmap(tagid)->flags & TAG_SPECIAL &&
			    (tagid != TAG_ADDRESS && tagid != TAG_DIV &&
			    tagid != TAG_P)) {
				goto dd_dt_done;
			} else if (sz > 1) {
				sz--;
				goto dd_dt_loop;
			} else {
				print_err(ctx, token, mode, "should not happen");
			}
			return STATE_NONE;
dd_dt_done:
			if (check_p(ctx, token, mode) == -1)
				return STATE_NONE;
			insert_tag(ctx, token);
			return STATE_NONE;
		}
		if (TOKEN_IS_START_TAG(token, TAG_PLAINTEXT)) {
			if (check_p(ctx, token, mode) == -1)
				return STATE_NONE;
			insert_tag(ctx, token);
			return STATE_PLAINTEXT;
		}
		if (TOKEN_IS_END_TAG(token, TAG_P)) {
			if (!has_element_in_scope(TAG_P, SCOPE_BUTTON)) {
				print_err(ctx, token, mode, "no p tag");
				insert_tag_name(ctx, "p", 0);
			}
			if (close_p_element(ctx) == 0) {
				print_err(ctx, token, mode, "closing p");
				return STATE_NONE;
			}
			return STATE_NONE;
		}
		if (TOKEN_IS_END_TAG(token, TAG_LI)) {
			if (!has_element_in_scope(TAG_LI, SCOPE_LIST_ITEM)) {
				print_err(ctx, token, mode, "no li tag");
				return STATE_NONE;
			}
			generate_implied_end_tags(ctx, TAG_LI);
			if (ostack_peek()->tagid != TAG_LI) {
				print_err(ctx, token, mode, "no match");
				return STATE_NONE;
			}
			pop_elem(ctx, TAG_LI);
			return STATE_NONE;
		}
		if (TOKEN_IS_START_TAG(token, TAG_LI)) {
li_loop:
			if (ostack_peek()->tagid == TAG_LI) {
				generate_implied_end_tags(ctx, TAG_LI);
				if (ostack_peek()->tagid != TAG_LI) {
					print_err(ctx, token, mode, "was not li tag");
					return STATE_NONE;
				}
				pop(ctx);
				goto li_done;
			}
			if ((tagmap(ostack_peek()->tagid)->flags & TAG_SPECIAL) &&
			    (ostack_peek()->tagid != TAG_ADDRESS &&
			     ostack_peek()->tagid != TAG_DIV &&
			     ostack_peek()->tagid != TAG_P))
				goto li_done;

			pop(ctx);
			goto li_loop;
li_done:
			if (check_p(ctx, token, mode) == -1)
				return STATE_NONE;
			insert_tag(ctx, token);
			return STATE_NONE;
		}
		break;
	case IMODE_AFTER_BODY:
		if (TOKEN_IS_END_TAG(token, TAG_HTML)) {
			insert_close_tag(ctx, token);
			ctx->mode = IMODE_AFTER_AFTER_BODY;
			warnx("[switch to after after body]");
			return STATE_NONE;
		}	
		break;
	case IMODE_IN_TABLE:
		if (TOKEN_IS_CHAR(token)) {
			ctx->orig_mode = ctx->mode;
			ctx->mode = IMODE_IN_TABLE_TEXT;
			return STATE_NONE;
		}
		if (TOKEN_IS_END_TAG(token, TAG_TABLE)) {
			if (!has_element_in_scope(TAG_TABLE, SCOPE_TABLE)) {
				print_err(ctx, token, mode, "no table tag");
				return STATE_NONE;
			}
			if (pop_elem(ctx, TAG_TABLE) == NULL) {
				print_err(ctx, token, mode, "did not pop table");
				return STATE_NONE;
			}
			assert(ctx->head_elem);
			ctx->mode = reset_imode(ctx->head_elem);
			return STATE_NONE;
		}
		if (is_start_tag(token, 3, TAG_TBODY, TAG_TFOOT, TAG_THEAD)) {
			clear_to_context(ctx, CONTEXT_TABLE);
			insert_tag_set_mode(ctx, token, IMODE_IN_TABLE_BODY);
			return STATE_NONE;
		}
		if (is_start_tag(token, 3, TAG_TD, TAG_TH, TAG_TR)) {
			clear_to_context(ctx, CONTEXT_TABLE);
			insert_tag_name_set_mode(ctx, "tbody", 0,
			    IMODE_IN_TABLE_BODY);
			return dispatch(ctx, token, ctx->begin, ctx->end);
		}
		return STATE_NONE;
		break;
	case IMODE_IN_TABLE_BODY:
		if (TOKEN_IS_START_TAG(token, TAG_TR)) {
			clear_to_context(ctx, CONTEXT_TABLE_BODY);
			insert_tag_set_mode(ctx, token, IMODE_IN_ROW);
			return STATE_NONE;
		}
		if (is_start_tag(token, 2, TAG_TH, TAG_TD)) {
			print_err(ctx, token, mode, "unexpected th/td");
			clear_to_context(ctx, CONTEXT_TABLE_BODY);
			insert_tag_name_set_mode(ctx, "tr", 0, IMODE_IN_ROW);
			return STATE_NONE;
		}
		return insert_token_with_mode(ctx, token, IMODE_IN_TABLE);
	case IMODE_IN_ROW:
		if (is_start_tag(token, 2, TAG_TH, TAG_TD)) {
			clear_to_context(ctx, CONTEXT_TABLE_ROW);
			insert_tag_set_mode(ctx, token, IMODE_IN_CELL);
			return STATE_NONE;
		}
		if (is_end_tag(token, 1, TAG_TR)) {
			if (!has_element_in_scope(TAG_TR, SCOPE_TABLE)) {
				print_err(ctx, token, mode, "no tr");
				return STATE_NONE;
			}
			clear_to_context(ctx, CONTEXT_TABLE_ROW);
			pop(ctx);
			ctx->mode = IMODE_IN_TABLE_BODY;
			return STATE_NONE;
		}
		if (is_start_tag(token, 7, TAG_CAPTION, TAG_COL, TAG_COLGROUP,
		    TAG_TBODY, TAG_TFOOT, TAG_THEAD, TAG_TR) ||
		    is_end_tag(token, 1, TAG_TABLE)) {
			if (!has_element_in_scope(TAG_TR, SCOPE_TABLE)) {
				print_err(ctx, token, mode, "no tr");
				return STATE_NONE;
			}
			clear_to_context(ctx, CONTEXT_TABLE_ROW);
			if (ostack_peek()->tagid != TAG_TR) {
				print_err(ctx, token, mode, "no tr");
				return STATE_NONE;
			}
			pop(ctx);
			ctx->mode = IMODE_IN_TABLE_BODY;
			return dispatch(ctx, token, ctx->begin, ctx->end);
		}
		return STATE_NONE;
	case IMODE_IN_CELL:
		if (is_end_tag(token, 2, TAG_TH, TAG_TD)) {
			if (!has_element_in_scope(token->u.tag.tagid,
			    SCOPE_TABLE)) {
				print_err(ctx, token, mode, "no th/td (in cell)");
				return STATE_NONE;
			}
			generate_implied_end_tags(ctx, -1);
			if (ostack_peek()->tagid != token->u.tag.tagid) {
				print_err(ctx, token, mode, "no th/td in cell 2");
				return STATE_NONE;
			}
			pop_elem(ctx, token->u.tag.tagid);
			ctx->mode = IMODE_IN_ROW;
			return STATE_NONE;
		}
		if (is_start_tag(token, 9, TAG_CAPTION, TAG_COL, TAG_COLGROUP,
		    TAG_TBODY, TAG_TD, TAG_TFOOT, TAG_TH, TAG_THEAD, TAG_TR)) {
			if (!has_element_in_scope(TAG_TD, SCOPE_TABLE) &&
			    !has_element_in_scope(TAG_TH, SCOPE_TABLE)) {
				print_err(ctx, token, mode, "no th/td (in cell)");
				return STATE_NONE;
			}
			close_cell(ctx, token);
			return dispatch(ctx, token, ctx->begin, ctx->end);	
		}
		if (is_end_tag(token, 5, TAG_BODY, TAG_CAPTION, TAG_COL,
		    TAG_COLGROUP, TAG_HTML)) {
			print_err(ctx, token, mode, "parse error");
			return STATE_NONE;
		}
		if (is_end_tag(token, 5, TAG_TABLE, TAG_TBODY, TAG_TFOOT,
		    TAG_THEAD, TAG_TR)) {
			if (!has_element_in_scope(token->u.tag.tagid,
			    SCOPE_TABLE)) {
				print_err(ctx, token, mode, "parse error");
				return STATE_NONE;
			}
			close_cell(ctx, token);
			return dispatch(ctx, token, ctx->begin, ctx->end);
		}
		return insert_token_with_mode(ctx, token, IMODE_IN_BODY);
	case IMODE_IN_TABLE_TEXT:
		if (TOKEN_IS_SPACE(token)) {
			insert_char(ctx, token);
			return STATE_NONE;
		} else {
			ctx->mode = ctx->orig_mode;
			return dispatch(ctx, token, ctx->begin, ctx->end);
		}
		return STATE_NONE;
		break;
	case IMODE_AFTER_AFTER_BODY:
		warnx("[in after after body]");
		break;
	case IMODE_IN_HEAD_NOSCRIPT:
		if (is_end_tag(token, 1, TAG_NOSCRIPT)) {
			pop(ctx);
		} else {
			pop(ctx);
		}
		ctx->mode = IMODE_IN_HEAD;
		break;
	default:
		if (TOKEN_IS_START(token)) {
			warnx("Got start tag: '%s'", token->u.tag.name);
		}
		return STATE_NONE;
	}

	/*
	 * Catch all non-special tag operations.
	 */
	switch (mode) {
	case IMODE_IN_HEAD:
		if (TOKEN_IS_START(token))
			insert_tag(ctx, token);
		else if (TOKEN_IS_END(token))
			insert_close_tag(ctx, token);
		break;
	case IMODE_IN_BODY:
		if (TOKEN_IS_START(token))
			insert_tag(ctx, token);
		else if (TOKEN_IS_END(token)) {
			/* "Any other end tag" */
			node = ostack_peek();
loop:
			if (node->ns == NS_HTML &&
			    node->tagid == token->u.tag.tagid) {
				generate_implied_end_tags(ctx,
				    token->u.tag.tagid);
				if (node->tagid != ostack_peek()->tagid) {
					print_err(ctx, token, mode,
					    "end tag did not match");
					return STATE_NONE;
				}
				while (ostack_depth() >= 1) {
					if (node == ostack_peek()) {
						pop(ctx);
						break;
					} else {
						pop(ctx);
					}
				}
				return STATE_NONE;
			} else if (tagmap(node->tagid)->flags &
			    TAG_SPECIAL) {
				warnx("special was: %s", node->name);
				print_err(ctx, token, mode, "was special");
				return STATE_NONE;
			} else {
				node = ostack_prev(node);
				if (node == NULL) {
					print_err(ctx, token, mode, "no prev node");
					return STATE_NONE;
				}
				goto loop;
			}
		}
		break;
	default:
		break;
	}

	return STATE_NONE;
}

/* helper */
static void
insert_token_set_mode(struct dispatcher *ctx, struct token *token, IMODE mode)
{
	ctx->mode = mode;
	insert_token_with_mode(ctx, token, ctx->mode);
}
