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

/*
 * Optional: getrusage() and gettimeofday() for perf display.
 */
#include <sys/resource.h>
#include <sys/time.h>

#include <purehtml/tokenize.h>
#include <purehtml/dispatch.h>
#include <purehtml/ostack.h>
#include <purehtml/node.h>
#include <purehtml/elem.h>
#include <purehtml/cdata.h>
#include <purehtml/document.h>

/*
 * We dump the tree as we get it.
 * The magic happens in begin() and end().
 */
static void begin(struct node *);
static void end(struct node *);

/*
 * Optional helper functions.
 */
static void print_stack(void);
static void print_text(struct str *);
static void print_indent(void);
static void print_val(size_t);
static void print_perf(struct timeval);
static void print_mem(void);

/*
 * Optional command line flags.
 */
static int want_stack;
static int want_reconstruct;
static int want_flat;
static int want_mem;
static int want_quiet;
static int want_perf;

/*
 * Optional summation of memory usage.
 */
static size_t cdata_mem;
static size_t elem_mem;

int
main(int argc, char **argv)
{
	static struct tokenizer tokenizer;
	static struct dispatcher dispatcher;
	static struct document document;
	FILE *fp;
	struct token *token;
	int state;
	char ch;
	struct timeval tv;

	gettimeofday(&tv, NULL);

	while ((ch = getopt(argc, argv, "srfmqp")) != -1) {
		switch (ch) {
		case 's':
			want_stack = 1;
			break;
		case 'r':
			want_reconstruct = 1;
			break;
		case 'f':
			want_flat = 1;
			break;
		case 'm':
			want_mem = 1;
			break;
		case 'q':
			want_quiet = 1;
			break;
		case 'p':
			want_perf = 1;
			break;
		default:
			fprintf(stderr,
			    "usage: %s [-srf] [file]\n"
			    "\t-s\tprint stack\n"
			    "\t-r\treconstruct HTML\n"
			    "\t-f\tprint flat without indent\n"
			    "\t-q\tquiet\n"
			    "\t-p\tshow performance metrics\n"
			    "\t-m\tsum memory usage\n",
			    *argv);
			return 1;
		}
	}

	argc -= optind;
	argv += optind;

	if (argc == 1) {
		fp = fopen(*argv, "r");
		if (fp == NULL)
			err(1, "fopen %s", *argv);
	} else {
		fp = fdopen(0, "r");
		if (fp == NULL)
			err(1, "fdopen stdin");
	}
	tokenizer.fp = fp;

	if (want_reconstruct)
		printf("<!DOCTYPE html>\n");

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

	if (want_perf)
		print_perf(tv);

	if (want_mem)
		print_mem();

	fclose(fp);
	return 0;
}

static void
print_mem()
{
	struct rusage ru;

	if (want_reconstruct)
		printf("<!--\n ");

	printf("mem\t");
	print_val(cdata_mem);
	printf(" cdata\n\t");
	print_val(elem_mem);
	printf(" elem\n\t");
	print_val(cdata_mem + elem_mem);
	printf(" total\n\t");

	getrusage(RUSAGE_SELF, &ru);
	print_val(ru.ru_maxrss * 1024);
	printf(" maxrss\n\t");

	printf("%4zu",
	    (size_t) (ru.ru_maxrss * 1024.0 / (cdata_mem + elem_mem)));
	printf(" maxrss factor");

	if (want_reconstruct)
		printf(" -->\n");
	else
		printf("\n");
}

static void
print_perf(struct timeval tv)
{
	struct rusage ru;
	struct timeval tvend;
	int real, total, system;

	getrusage(RUSAGE_SELF, &ru);
	gettimeofday(&tvend, NULL);

	if (want_reconstruct)
		printf("<!--\n ");

	real = (int) (((tvend.tv_sec * 1000000 + tvend.tv_usec) -
	    (tv.tv_sec * 1000000 + tv.tv_usec)) / 1000);
	total = (int) (
	    ((ru.ru_utime.tv_sec * 1000000) + (ru.ru_stime.tv_sec * 1000000) +
	    ru.ru_utime.tv_usec + ru.ru_stime.tv_usec) / 1000);
	system = (int) ((ru.ru_stime.tv_sec * 1000000) +
	    (ru.ru_stime.tv_usec) / 1000);

	printf("rutime\t%6d ms\n\t%6d ms system",
	    total, system);
	printf("\n\t%6d ms unaccounted latencies",
	    (real - (total+system)) > 0 ?
	    (real - (total+system)) : 0);

	if (want_reconstruct)
		printf(" -->\n");
	else
		printf("\n");
}

static void
print_stack()
{
	size_t	 i;
	size_t	 sz;

	putchar('\t');

	if (want_reconstruct)
		printf("<!-- ");

	sz = ostack_depth();
	for (i = sz; i >= 1; i--) {
		if (i != sz)
			printf(".");

		printf("%s", ostack_peek_at(i)->name);
	}

	if (want_reconstruct)
		printf("-->");
}

static void
print_text(struct str *s)
{
	const char *p;

	if (!want_reconstruct) {
		printf("#text");
		if (want_mem)
			printf("(%zu/%zu): ", s->len, s->alloc);
		else
			printf(": ");
	}

	p = s->s;
	while (*p != '\0') {
		if (!want_reconstruct && *p == '\n')
			putchar('$');
		else if (!want_reconstruct || *p != '\n')
			putchar(*p);
		p++;
	}

	if (!want_reconstruct)
		printf("\"");
}

static void
print_indent()
{
	size_t i;

	for (i = ostack_depth(); i >= 1; i--)
		putchar(' ');
}

static void
begin(struct node *node)
{
	char *p;

	if (!want_flat && !want_quiet)
		print_indent();

	switch (node->type) {
	case NODE_ELEM:
		if (want_reconstruct && !want_quiet)
			putchar('<');

		p = node->u.elem->name;
		while (*p != '\0') {
			if (want_reconstruct && !want_quiet)
				putchar(tolower(*p));
			else if (!want_quiet)
				putchar(toupper(*p));
			p++;
		}

		if (want_reconstruct && !want_quiet)
			putchar('>');

		if (!want_quiet)
			putchar(' ');

		if (want_mem)
			elem_mem += node_size(node);
		break;
	case NODE_CDATA:
		if (!want_quiet)
			print_text(&node->u.cdata->data);

		if (want_mem)
			cdata_mem += node_size(node);

		node_free(node);
		break;
	default:
		break;
	}

	if (want_stack && !want_quiet)
		print_stack();
	if (!want_quiet)
		putchar('\n');

#if 0
	node_free(node);
#endif
}

static void
end(struct node *node)
{
	if (!want_reconstruct && node->type == NODE_ELEM) {
		node_free(node);
		return;
	}

	if (!want_flat && !want_quiet)
		print_indent();

	switch (node->type) {
	case NODE_ELEM:
		if (want_reconstruct && !want_quiet)
			printf("</%s> ", node->u.elem->name);
		break;
	case NODE_CDATA:
		if (!want_quiet)
			print_text(&node->u.cdata->data);

		if (want_mem)
			cdata_mem += node_size(node);
		break;
	default:
		break;
	}

	if (want_stack && !want_quiet)
		print_stack();
	if (!want_quiet)
		putchar('\n');

	node_free(node);
}

static void
print_val(size_t val)
{
		if (val > 1024 * 1024)
			printf("%4zu MiB", 1 + (val / 1024 / 1024));
		else if (val > 1024)
			printf("%4zu KiB", 1 + (val / 1024));
		else
			printf("%4zu", val);
}
