/* ui-log.c: functions for log output
 *
 * Copyright (C) 2006 Lars Hjemli
 *
 * Licensed under GNU General Public License v2
 *   (see COPYING for full license text)
 */

#include "cgit.h"
#include "html.h"
#include "ui-shared.h"
#include "vector.h"

int files, add_lines, rem_lines;

/*
 * The list of available column colors in the commit graph.
 */
static const char *column_colors_html[] = {
	"<span class='column1'>",
	"<span class='column2'>",
	"<span class='column3'>",
	"<span class='column4'>",
	"<span class='column5'>",
	"<span class='column6'>",
	"</span>",
};

#define COLUMN_COLORS_HTML_MAX (ARRAY_SIZE(column_colors_html) - 1)

void count_lines(char *line, int size)
{
	if (size <= 0)
		return;

	if (line[0] == '+')
		add_lines++;

	else if (line[0] == '-')
		rem_lines++;
}

void inspect_files(struct diff_filepair *pair)
{
	unsigned long old_size = 0;
	unsigned long new_size = 0;
	int binary = 0;

	files++;
	if (ctx.repo->enable_log_linecount)
		cgit_diff_files(pair->one->sha1, pair->two->sha1, &old_size,
				&new_size, &binary, 0, ctx.qry.ignorews,
				count_lines);
}

void show_commit_decorations(struct commit *commit)
{
	struct name_decoration *deco;
	static char buf[1024];

	buf[sizeof(buf) - 1] = 0;
	deco = lookup_decoration(&name_decoration, &commit->object);
	while (deco) {
		if (!prefixcmp(deco->name, "refs/heads/")) {
			strncpy(buf, deco->name + 11, sizeof(buf) - 1);
			cgit_log_link(buf, NULL, "branch-deco", buf, NULL,
				      ctx.qry.vpath, 0, NULL, NULL,
				      ctx.qry.showmsg);
		}
		else if (!prefixcmp(deco->name, "tag: refs/tags/")) {
			strncpy(buf, deco->name + 15, sizeof(buf) - 1);
			cgit_tag_link(buf, NULL, "tag-deco", ctx.qry.head, buf);
		}
		else if (!prefixcmp(deco->name, "refs/tags/")) {
			strncpy(buf, deco->name + 10, sizeof(buf) - 1);
			cgit_tag_link(buf, NULL, "tag-deco", ctx.qry.head, buf);
		}
		else if (!prefixcmp(deco->name, "refs/remotes/")) {
			strncpy(buf, deco->name + 13, sizeof(buf) - 1);
			cgit_log_link(buf, NULL, "remote-deco", NULL,
				      sha1_to_hex(commit->object.sha1),
				      ctx.qry.vpath, 0, NULL, NULL,
				      ctx.qry.showmsg);
		}
		else {
			strncpy(buf, deco->name, sizeof(buf) - 1);
			cgit_commit_link(buf, NULL, "deco", ctx.qry.head,
					 sha1_to_hex(commit->object.sha1),
					 ctx.qry.vpath, 0);
		}
		deco = deco->next;
	}
}

void print_commit(struct commit *commit, struct rev_info *revs)
{
	struct commitinfo *info;
	char *tmp;
	int cols = 2;
	struct strbuf graphbuf = STRBUF_INIT;

	if (ctx.repo->enable_log_filecount) {
		cols++;
		if (ctx.repo->enable_log_linecount)
			cols++;
	}

	if (revs->graph) {
		/* Advance graph until current commit */
		while (!graph_next_line(revs->graph, &graphbuf)) {
			/* Print graph segment in otherwise empty table row */
			html("<tr class='nohover'><td/><td class='commitgraph'>");
			html(graphbuf.buf);
			htmlf("</td><td colspan='%d' /></tr>\n", cols);
			strbuf_setlen(&graphbuf, 0);
		}
		/* Current commit's graph segment is now ready in graphbuf */
	}

	info = cgit_parse_commit(commit);
	htmlf("<tr%s><td>",
		ctx.qry.showmsg ? " class='logheader'" : "");
	tmp = fmt("id=%s", sha1_to_hex(commit->object.sha1));
	tmp = cgit_fileurl(ctx.repo->url, "commit", ctx.qry.vpath, tmp);
	html_link_open(tmp, NULL, NULL);
	cgit_print_age(commit->date, TM_WEEK * 2, FMT_SHORTDATE);
	html_link_close();
	html("</td>");

	if (revs->graph) {
		/* Print graph segment for current commit */
		html("<td class='commitgraph'>");
		html(graphbuf.buf);
		html("</td>");
		strbuf_setlen(&graphbuf, 0);
	}

	htmlf("<td%s>", ctx.qry.showmsg ? " class='logsubject'" : "");
	cgit_commit_link(info->subject, NULL, NULL, ctx.qry.head,
			 sha1_to_hex(commit->object.sha1), ctx.qry.vpath, 0);
	show_commit_decorations(commit);
	html("</td><td>");
	html_txt(info->author);
	if (ctx.repo->enable_log_filecount) {
		files = 0;
		add_lines = 0;
		rem_lines = 0;
		cgit_diff_commit(commit, inspect_files, ctx.qry.vpath);
		html("</td><td>");
		htmlf("%d", files);
		if (ctx.repo->enable_log_linecount) {
			html("</td><td>");
			htmlf("-%d/+%d", rem_lines, add_lines);
		}
	}
	html("</td></tr>\n");

	if (revs->graph || ctx.qry.showmsg) { /* Print a second table row */
		struct strbuf msgbuf = STRBUF_INIT;
		html("<tr class='nohover'><td/>"); /* Empty 'Age' column */

		if (ctx.qry.showmsg) {
			/* Concatenate commit message + notes in msgbuf */
			if (info->msg && *(info->msg)) {
				strbuf_addstr(&msgbuf, info->msg);
				strbuf_addch(&msgbuf, '\n');
			}
			format_note(NULL, commit->object.sha1, &msgbuf,
			            PAGE_ENCODING,
			            NOTES_SHOW_HEADER | NOTES_INDENT);
			strbuf_addch(&msgbuf, '\n');
			strbuf_ltrim(&msgbuf);
		}

		if (revs->graph) {
			int lines = 0;

			/* Calculate graph padding */
			if (ctx.qry.showmsg) {
				/* Count #lines in commit message + notes */
				const char *p = msgbuf.buf;
				lines = 1;
				while ((p = strchr(p, '\n'))) {
					p++;
					lines++;
				}
			}

			/* Print graph padding */
			html("<td class='commitgraph'>");
			while (lines > 0 || !graph_is_commit_finished(revs->graph)) {
				if (graphbuf.len)
					html("\n");
				strbuf_setlen(&graphbuf, 0);
				graph_next_line(revs->graph, &graphbuf);
				html(graphbuf.buf);
				lines--;
			}
			html("</td>\n");
		}

		/* Print msgbuf into remainder of table row */
		htmlf("<td colspan='%d'%s>\n", cols,
			ctx.qry.showmsg ? " class='logmsg'" : "");
		html_txt(msgbuf.buf);
		html("</td></tr>\n");
		strbuf_release(&msgbuf);
	}

	strbuf_release(&graphbuf);
	cgit_free_commitinfo(info);
}

static const char *disambiguate_ref(const char *ref)
{
	unsigned char sha1[20];
	const char *longref;

	longref = fmt("refs/heads/%s", ref);
	if (get_sha1(longref, sha1) == 0)
		return longref;

	return ref;
}

static char *next_token(char **src)
{
	char *result;

	if (!src || !*src)
		return NULL;
	while (isspace(**src))
		(*src)++;
	if (!**src)
		return NULL;
	result = *src;
	while (**src) {
		if (isspace(**src)) {
			**src = '\0';
			(*src)++;
			break;
		}
		(*src)++;
	}
	return result;
}

void cgit_print_log(const char *tip, int ofs, int cnt, char *grep, char *pattern,
		    char *path, int pager)
{
	struct rev_info rev;
	struct commit *commit;
	struct vector vec = VECTOR_INIT(char *);
	int i, columns = 3;
	char *arg;

	/* First argv is NULL */
	vector_push(&vec, NULL, 0);

	if (!tip)
		tip = ctx.qry.head;
	tip = disambiguate_ref(tip);
	vector_push(&vec, &tip, 0);

	if (grep && pattern && *pattern) {
		pattern = xstrdup(pattern);
		if (!strcmp(grep, "grep") || !strcmp(grep, "author") ||
		    !strcmp(grep, "committer")) {
			arg = fmt("--%s=%s", grep, pattern);
			vector_push(&vec, &arg, 0);
		}
		if (!strcmp(grep, "range")) {
			/* Split the pattern at whitespace and add each token
			 * as a revision expression. Do not accept other
			 * rev-list options. Also, replace the previously
			 * pushed tip (it's no longer relevant).
			 */
			vec.count--;
			while ((arg = next_token(&pattern))) {
				if (*arg == '-') {
					fprintf(stderr, "Bad range expr: %s\n",
						arg);
					break;
				}
				vector_push(&vec, &arg, 0);
			}
		}
	}
	if (ctx.repo->enable_commit_graph) {
		static const char *graph_arg = "--graph";
		static const char *color_arg = "--color";
		vector_push(&vec, &graph_arg, 0);
		vector_push(&vec, &color_arg, 0);
		graph_set_column_colors(column_colors_html,
					COLUMN_COLORS_HTML_MAX);
	}

	if (path) {
		arg = "--";
		vector_push(&vec, &arg, 0);
		vector_push(&vec, &path, 0);
	}

	/* Make sure the vector is NULL-terminated */
	vector_push(&vec, NULL, 0);
	vec.count--;

	init_revisions(&rev, NULL);
	rev.abbrev = DEFAULT_ABBREV;
	rev.commit_format = CMIT_FMT_DEFAULT;
	rev.verbose_header = 1;
	rev.show_root_diff = 0;
	setup_revisions(vec.count, vec.data, &rev, NULL);
	load_ref_decorations(DECORATE_FULL_REFS);
	rev.show_decorations = 1;
	rev.grep_filter.regflags |= REG_ICASE;
	compile_grep_patterns(&rev.grep_filter);
	prepare_revision_walk(&rev);

	if (pager)
		html("<table class='list nowrap'>");

	html("<tr class='nohover'><th class='left'>Age</th>");
	if (ctx.repo->enable_commit_graph)
		html("<th></th>");
	html("<th class='left'>Commit message");
	if (pager) {
		html(" (");
		cgit_log_link(ctx.qry.showmsg ? "Collapse" : "Expand", NULL,
			      NULL, ctx.qry.head, ctx.qry.sha1,
			      ctx.qry.vpath, ctx.qry.ofs, ctx.qry.grep,
			      ctx.qry.search, ctx.qry.showmsg ? 0 : 1);
		html(")");
	}
	html("</th><th class='left'>Author</th>");
	if (ctx.repo->enable_log_filecount) {
		html("<th class='left'>Files</th>");
		columns++;
		if (ctx.repo->enable_log_linecount) {
			html("<th class='left'>Lines</th>");
			columns++;
		}
	}
	html("</tr>\n");

	if (ofs<0)
		ofs = 0;

	for (i = 0; i < ofs && (commit = get_revision(&rev)) != NULL; i++) {
		free(commit->buffer);
		commit->buffer = NULL;
		free_commit_list(commit->parents);
		commit->parents = NULL;
	}

	for (i = 0; i < cnt && (commit = get_revision(&rev)) != NULL; i++) {
		print_commit(commit, &rev);
		free(commit->buffer);
		commit->buffer = NULL;
		free_commit_list(commit->parents);
		commit->parents = NULL;
	}
	if (pager) {
		html("</table><div class='pager'>");
		if (ofs > 0) {
			cgit_log_link("[prev]", NULL, NULL, ctx.qry.head,
				      ctx.qry.sha1, ctx.qry.vpath,
				      ofs - cnt, ctx.qry.grep,
				      ctx.qry.search, ctx.qry.showmsg);
			html("&nbsp;");
		}
		if ((commit = get_revision(&rev)) != NULL) {
			cgit_log_link("[next]", NULL, NULL, ctx.qry.head,
				      ctx.qry.sha1, ctx.qry.vpath,
				      ofs + cnt, ctx.qry.grep,
				      ctx.qry.search, ctx.qry.showmsg);
		}
		html("</div>");
	} else if ((commit = get_revision(&rev)) != NULL) {
		html("<tr class='nohover'><td colspan='3'>");
		cgit_log_link("[...]", NULL, NULL, ctx.qry.head, NULL,
			      ctx.qry.vpath, 0, NULL, NULL, ctx.qry.showmsg);
		html("</td></tr>\n");
	}
}
