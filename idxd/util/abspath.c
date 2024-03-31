/* SPDX-License-Identifier: GPL-2.0 */
/* originally copied from git/abspath.c */

#include <util/util.h>
#include <util/strbuf.h>

char *prefix_filename(const char *pfx, const char *arg)
{
	struct strbuf path = STRBUF_INIT;

	if (pfx && !is_absolute_path(arg))
		strbuf_add(&path, pfx, strlen(pfx));

	strbuf_addstr(&path, arg);
	return strbuf_detach(&path, NULL);
}

void fix_filename(const char *prefix, const char **file)
{
	if (!file || !*file || !prefix || is_absolute_path(*file)
			|| !strcmp("-", *file))
		return;
	*file = prefix_filename(prefix, *file);
}
