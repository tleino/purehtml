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

#include <ctype.h>
#include <err.h>
#include <unistd.h>
#include <assert.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>

#include <purehtml/tokenize.h>
#include <purehtml/dispatch.h>
#include <purehtml/ostack.h>
#include <purehtml/node.h>
#include <purehtml/elem.h>
#include <purehtml/cdata.h>
#include <purehtml/document.h>
#include <purehtml/util.h>
#include <purehtml/tagmap.h>

/*
 * Data structure for storing links. We store them separately because
 * in 'text/gemtext' we display them after content blocks.
 */
struct link {
	int is_img;
	char *desc;
	char *url;
	struct block *block;
	struct link *next;
};

/*
 * Block container which gathers text data from inline elements for
 * 'display: block;' HTML elements.
 */
struct block {
	int tagid;
	struct str s;
	struct block *prev;
	int has_content;	/* i.e. non-whitespace content */
};

/*
 * Current block element. We support nesting of block elements.
 */
static struct block *current_block;

/*
 * Linked list of links.
 */
static struct link *link_head;

/*
 * Current link text that we are constructing.
 */
static struct str link_text;

/*
 * Do we have links waiting to be dumped?
 * Did we already add LF?
 *
 * These tricks are required because we want special handling for
 * <li> and <ul>, <ol> tags for getting links better bundled together
 * instead of being dumped after each list item.
 *
 * We also defer dumping of links of <h1-6> blocks so that when
 * heading elements are used as a navigation aid (e.g. in Table of
 * Contents), the links are bundled together just like in the <li>
 * case.
 *
 * We also dump links only after a final flush of a block element,
 * not in between of nested block elements.
 */
static int have_links;
static int have_lf;

/*
 * We convert to 'text/gemini' as we get it.
 * The magic happens here.
 */
static void begin(struct node *);
static void end(struct node *);

/*
 * Print content properly according to HTML inline rendering context
 * whitespace rules.
 */
static void print_content(const char *);
static void print_line(const char *, size_t, char);

/*
 * Support for storing links.
 */
static int has_link(const char *);
static void add_link(struct block *, const char *, const char *, int);

/*
 * General helpers.
 */
static int is_child_of(int);

/*
 * Print as 'text/gemini'.
 */
static void print_heading(int, const char *);
static void print_bullet(const char *);
static void print_blockquote(const char *);
static void print_generic_block(const char *);

/*
 * Support for blocks.
 */
static void begin_block(struct node *);
static void end_block(struct node *);
static void block_add_text(struct block *, const char *);
static void flush_block(struct block *, int);
static void block_free(struct block *);

/*
 * Support for links.
 */
static void link_add_text(const char *);
static int flush_links(struct block *);
static void free_links(void);

/*
 * Extracting contents from elements.
 */
static void begin_a(void);
static void end_br(void);
static void end_img(struct elem *);
static void end_a(struct elem *);

int
main(int argc, char **argv)
{
	static struct tokenizer tokenizer;
	static struct dispatcher dispatcher;
	static struct document document;
	FILE *fp;
	struct token *token;
	int state;

	if (argc == 2) {
		fp = fopen(argv[1], "r");
		if (fp == NULL)
			err(1, "fopen %s", argv[1]);
	} else {
		fp = fdopen(0, "r");
		if (fp == NULL)
			err(1, "fdopen stdin");
	}
	tokenizer.fp = fp;

	dispatcher.document = &document;
	while (!feof(fp)) {
		token = tokenize(&tokenizer);
		if (token != NULL) {
			state = dispatch(&dispatcher, token, begin, end);
			if (state != STATE_NONE)
				tokenizer.state = state;
			token_clear(token);
		}
	}

	free_links();
	fclose(fp);
	return 0;
}

static void
begin(struct node *node)
{
	switch (node->type) {
	case NODE_ELEM:
		if (tagmap(node->u.elem->tagid)->flags & TAG_BLOCK)
			begin_block(node);
		else {
			switch(node->u.elem->tagid) {
			case TAG_A:
				begin_a();
				break;
			default:
				break;
			}
		}
		break;
	case NODE_CDATA:
		block_add_text(current_block, node->u.cdata->data.s);

		if (is_child_of(TAG_A))
			link_add_text(node->u.cdata->data.s);

		node_free(node);
		break;
	default:
		break;
	}
}

static void
end(struct node *node)
{
	switch (node->type) {
	case NODE_ELEM:
		if (tagmap(node->u.elem->tagid)->flags & TAG_BLOCK)
			end_block(node);
		else {
			switch(node->u.elem->tagid) {
			case TAG_BR:
				end_br();
				break;
			case TAG_IMG:
				end_img(node->u.elem);
				break;
			case TAG_A:
				end_a(node->u.elem);
				break;
			}
		}
		break;
	case NODE_CDATA:
		block_add_text(current_block, node->u.cdata->data.s);

		if (is_child_of(TAG_A))
			link_add_text(node->u.cdata->data.s);
		break;
	default:
		break;
	}

	node_free(node);
}

static int
is_child_of(int tagid)
{
	struct elem *elem;

	elem = ostack_peek();
	if (elem == NULL)
		return 0;

	if (elem->tagid == tagid)
		return 1;

	while ((elem = ostack_prev(elem)) != NULL)
		if (elem->tagid == tagid)
			return 1;

	return 0;
}

static int
has_link(const char *url)
{
	struct link *link;

	link = link_head;
	while (link != NULL) {
		if (link->url != NULL && strcmp(link->url, url) == 0)
			return 1;
		link = link->next;
	}

	return 0;
}

static void
add_link(struct block *block, const char *url, const char *desc, int is_img)
{
	struct link *link;

	if (url == NULL || *url == '\0' || *url == '#')
		return;

	if (has_link(url))
		return;

	link = calloc(1, sizeof(struct link));
	if (link == NULL)
		err(1, "calloc link");

	link->block = block;
	have_links = 1;

	link->is_img = is_img;

	link->url = strdup(url);
	if (link->url == NULL)
		err(1, "strdup url");

	if (desc != NULL && *desc != '\0') {
		link->desc = strdup(desc);
		if (link->desc == NULL)
			err(1, "strdup desc");
	}

	link->next = link_head;
	link_head = link;
}

/*
 * The input is already stripped out of CRs by purehtml, so we can use
 * CR here to signify the BR element. So, if we get CR, we dump it as LF
 * immediately.
 */
static void
print_line(const char *s, size_t len, char last)
{
	char c;

	while (*s != '\0' && isspace(*s) && len > 0) {
		s++;
		len--;
	}

	c = '\0';
	while (len-- && *s != '\0') {
		if (*s == '\r') {
		} else if (isspace(c) && isspace(*s)) {
			c = *s++;
			continue;
		}
		putchar((c = *s++));
	}

	if (last != '\0' && !isspace(c) && last == '\n')
		putchar(' ');
}

static void
print_content(const char *s)
{
	const char *p;
	const char *begin;
	const char *last_solid;

	if (s == NULL)
		return;

	p = s;
	while (*p != '\0') {

		begin = p;
		last_solid = NULL;
		while (*p != '\0' && *p != '\n') {
			if (!isspace(*p))
				last_solid = p++;
			else
				p++;
		}

		if (last_solid == NULL && p > begin)
			last_solid = p-1;
		else if (last_solid == NULL)
			last_solid = p;

		if (p > begin)
			print_line(begin, (last_solid - begin) + 1, *p);

		while (*p != '\0' && isspace(*p))
			p++;
	}
}

static void
print_heading(int tagid, const char *s)
{
	switch (tagid) {
	case TAG_H1:
		printf("# ");
		break;
	case TAG_H2:
		printf("## ");
		break;
	default:
		printf("### ");
		break;
	}

	print_content(s);
	putchar('\n');
}

static void
print_bullet(const char *s)
{
	printf("* ");
	print_content(s);
	putchar('\n');
}

static void
print_blockquote(const char *s)
{
	printf("> ");
	print_content(s);
	putchar('\n');
}

static void
print_generic_block(const char *s)
{
	print_content(s);
	putchar('\n');
}

static void
free_links()
{
	struct link *link;
	struct link *tmp;

	link = link_head;
	while (link != NULL) {
		tmp = link;
		link = link->next;

		if (tmp->url != NULL)
			free(tmp->url);
		if (tmp->desc != NULL)
			free(tmp->desc);
		free(tmp);
	}	
}

static int
flush_links(struct block *block)
{
	struct link *link;
	size_t i;

	i = 0;
	link = link_head;
	for (link = link_head; link != NULL; link = link->next) {
		if (link->block == NULL)
			continue;

		link->block = NULL;
		printf("=> %s", link->url);
		if (link->desc != NULL && *link->desc != '\0') {
			putchar(' ');
			print_content(link->desc);
		}
		printf("\n");
		i++;
	}
	have_links = 0;

	return i;
}

static void
flush_block_links(struct block *block, int final_flush)
{
	if (final_flush &&
	    !(block->tagid == TAG_LI ||
	    tagmap(block->tagid)->flags & TAG_HEADING)) {
		if (!block->has_content && have_links && !have_lf) {
			putchar('\n');
			have_lf = 1;
		}

		if (flush_links(block) > 0)
			putchar('\n');
	}
}

static void
flush_block(struct block *block, int final_flush)
{
	if (block->has_content == 0) {
		str_add(&block->s, '\0');

		if (have_links) {
			flush_block_links(block, final_flush);
		} else if (final_flush &&
		    (block->tagid == TAG_OL || block->tagid == TAG_UL)) {
			printf("\n");
		}

		return;
	}

	have_lf = 0;
	if (tagmap(block->tagid)->flags & TAG_HEADING)
		print_heading(block->tagid, block->s.s);
	else if (block->tagid == TAG_LI)
		print_bullet(block->s.s);
	else if (block->tagid == TAG_BLOCKQUOTE)
		print_blockquote(block->s.s);
	else
		print_generic_block(block->s.s);

	if (block->tagid != TAG_LI) {
		putchar('\n');
		have_lf = 1;
	}

	flush_block_links(block, final_flush);

	str_add(&block->s, '\0');
	block->has_content = 0;
}

static void
begin_block(struct node *node)
{
	struct block *block;

	assert(node->type == NODE_ELEM);
	if (node->type != NODE_ELEM)
		return;

	if (current_block != NULL)
		flush_block(current_block, 0);

	block = calloc(1, sizeof(struct block));
	if (block == NULL)
		err(1, "calloc block");

	block->tagid = node->u.elem->tagid;
	block->prev = current_block;
	current_block = block;
}

static void
block_free(struct block *block)
{
	if (block->s.s != NULL)
		free(block->s.s);
	free(block);
}

static void
end_block(struct node *node)
{
	struct block *tmp;

	assert(current_block != NULL);
	if (current_block == NULL)
		return;

	flush_block(current_block, 1);

	tmp = current_block;
	current_block = current_block->prev;

	block_free(tmp);
}

static void
block_add_text(struct block *block, const char *s)
{
	if (block == NULL)
		return;

	while (*s != '\0') {
		if (!isspace(*s))
			block->has_content = 1;
		str_add(&block->s, *s++);
	}
}

static void
link_add_text(const char *s)
{
	while (*s)
		str_add(&link_text, *s++);
}

static void
begin_a()
{
	str_add(&link_text, '\0');	
}

static void
end_br()
{
	/*
	 * We don't do br instead of li elements, or we'll break the
	 * gemtext format.
	 */
	if (is_child_of(TAG_LI))
		return;

	/*
	 * LF will be stripped if it appears in
	 * the begin or end. If we're at begin,
	 * we forcibly add LF here.
	 */
	if (!current_block->has_content) {
		putchar('\n');
	} else
		block_add_text(current_block, "\r");
}

static void
end_img(struct elem *elem)
{
	const char *src;
	const char *alt;

	src = elem_attr_value(elem, "src");
	alt = elem_attr_value(elem, "alt");

	add_link(current_block, src, alt, 1);
}

static void
end_a(struct elem *elem)
{
	const char *href;

	href = elem_attr_value(elem, "href");

	add_link(current_block, href, link_text.s, 0);
}
