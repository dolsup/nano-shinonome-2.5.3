/* $Id: color.c 5538 2016-01-09 18:41:56Z bens $ */
/**************************************************************************
 *   color.c                                                              *
 *                                                                        *
 *   Copyright (C) 2001, 2002, 2003, 2004, 2005, 2006, 2007, 2008, 2009,  *
 *   2010, 2011, 2013, 2014, 2015 Free Software Foundation, Inc.          *
 *   This program is free software; you can redistribute it and/or modify *
 *   it under the terms of the GNU General Public License as published by *
 *   the Free Software Foundation; either version 3, or (at your option)  *
 *   any later version.                                                   *
 *                                                                        *
 *   This program is distributed in the hope that it will be useful, but  *
 *   WITHOUT ANY WARRANTY; without even the implied warranty of           *
 *   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU    *
 *   General Public License for more details.                             *
 *                                                                        *
 *   You should have received a copy of the GNU General Public License    *
 *   along with this program; if not, write to the Free Software          *
 *   Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA            *
 *   02110-1301, USA.                                                     *
 *                                                                        *
 **************************************************************************/

#include "proto.h"

#include <stdio.h>
#include <string.h>
#include <errno.h>
#include <time.h>
#include <unistd.h>

#ifdef HAVE_MAGIC_H
#include <magic.h>
#endif

#ifndef DISABLE_COLOR

/* Initialize the colors for nano's interface, and assign pair numbers
 * for the colors in each syntax. */
void set_colorpairs(void)
{
    const syntaxtype *this_syntax = syntaxes;
    bool using_defaults = FALSE;
    short foreground, background;
    size_t i;

    /* Tell ncurses to enable colors. */
    start_color();

#ifdef HAVE_USE_DEFAULT_COLORS
    /* Allow using the default colors, if available. */
    using_defaults = (use_default_colors() != ERR);
#endif

    /* Initialize the color pairs for nano's interface elements. */
    for (i = 0; i < NUMBER_OF_ELEMENTS; i++) {
	bool bright = FALSE;

	if (parse_color_names(specified_color_combo[i],
			&foreground, &background, &bright)) {
	    if (foreground == -1 && !using_defaults)
		foreground = COLOR_WHITE;
	    if (background == -1 && !using_defaults)
		background = COLOR_BLACK;
	    init_pair(i + 1, foreground, background);
	    interface_color_pair[i].bright = bright;
	    interface_color_pair[i].pairnum = COLOR_PAIR(i + 1);
	}
	else {
	    interface_color_pair[i].bright = FALSE;
	    if (i != FUNCTION_TAG)
		interface_color_pair[i].pairnum = hilite_attribute;
	    else
		interface_color_pair[i].pairnum = A_NORMAL;
	}

	free(specified_color_combo[i]);
	specified_color_combo[i] = NULL;
    }

    /* For each syntax, go through its list of colors and assign each
     * its pair number, giving identical color pairs the same number. */
    for (; this_syntax != NULL; this_syntax = this_syntax->next) {
	colortype *this_color = this_syntax->color;
	int clr_pair = NUMBER_OF_ELEMENTS + 1;

	for (; this_color != NULL; this_color = this_color->next) {
	    const colortype *beforenow = this_syntax->color;

	    while (beforenow != this_color &&
			(beforenow->fg != this_color->fg ||
			beforenow->bg != this_color->bg ||
			beforenow->bright != this_color->bright))
		beforenow = beforenow->next;

	    if (beforenow != this_color)
		this_color->pairnum = beforenow->pairnum;
	    else {
		this_color->pairnum = clr_pair;
		clr_pair++;
	    }
	}
    }
}

/* Initialize the color information. */
void color_init(void)
{
    colortype *tmpcolor = openfile->colorstrings;
    bool using_defaults = FALSE;
    short foreground, background;

    assert(openfile != NULL);

    /* If the terminal is not capable of colors, forget it. */
    if (!has_colors())
	return;

#ifdef HAVE_USE_DEFAULT_COLORS
    /* Allow using the default colors, if available. */
    using_defaults = (use_default_colors() != ERR);
#endif

    /* For each coloring expression, initialize the color pair. */
    for (; tmpcolor != NULL; tmpcolor = tmpcolor->next) {
	foreground = tmpcolor->fg;
	background = tmpcolor->bg;

	if (foreground == -1 && !using_defaults)
	    foreground = COLOR_WHITE;

	if (background == -1 && !using_defaults)
	    background = COLOR_BLACK;

	init_pair(tmpcolor->pairnum, foreground, background);
#ifdef DEBUG
	fprintf(stderr, "init_pair(): fg = %hd, bg = %hd\n", foreground, background);
#endif
    }
}

/* Clean up a regex we previously compiled. */
void nfreeregex(regex_t **r)
{
    assert(r != NULL);

    regfree(*r);
    free(*r);
    *r = NULL;
}

/* Update the color information based on the current filename. */
void color_update(void)
{
    syntaxtype *tmpsyntax;
    syntaxtype *defsyntax = NULL;
    colortype *tmpcolor, *defcolor = NULL;
    regexlisttype *e;

    assert(openfile != NULL);

    openfile->syntax = NULL;
    openfile->colorstrings = NULL;

    /* If the rcfiles were not read, or contained no syntaxes, get out. */
    if (syntaxes == NULL)
	return;

    /* If we specified a syntax override string, use it. */
    if (syntaxstr != NULL) {
	/* If the syntax override is "none", it's the same as not having
	 * a syntax at all, so get out. */
	if (strcmp(syntaxstr, "none") == 0)
	    return;

	for (tmpsyntax = syntaxes; tmpsyntax != NULL;
		tmpsyntax = tmpsyntax->next) {
	    if (strcmp(tmpsyntax->desc, syntaxstr) == 0) {
		openfile->syntax = tmpsyntax;
		openfile->colorstrings = tmpsyntax->color;
	    }

	    if (openfile->colorstrings != NULL)
		break;
	}

	if (openfile->colorstrings == NULL)
	    statusbar(_("Unknown syntax name: %s"), syntaxstr);
    }

    /* If we didn't specify a syntax override string, or if we did and
     * there was no syntax by that name, get the syntax based on the
     * file extension, then try the headerline, and then try magic. */
    if (openfile->colorstrings == NULL) {
	char *currentdir = getcwd(NULL, PATH_MAX + 1);
	char *joinednames = charalloc(PATH_MAX + 1);
	char *fullname = NULL;

	if (currentdir != NULL) {
	    /* Concatenate the current working directory with the
	     * specified filename, and canonicalize the result. */
	    sprintf(joinednames, "%s/%s", currentdir, openfile->filename);
	    fullname = realpath(joinednames, NULL);
	    free(currentdir);
	}

	if (fullname == NULL)
	    fullname = mallocstrcpy(fullname, openfile->filename);

	for (tmpsyntax = syntaxes; tmpsyntax != NULL;
		tmpsyntax = tmpsyntax->next) {

	    /* If this is the default syntax, it has no associated
	     * extensions, which we've checked for elsewhere.  Skip over
	     * it here, but keep track of its color regexes. */
	    if (strcmp(tmpsyntax->desc, "default") == 0) {
		defsyntax = tmpsyntax;
		defcolor = tmpsyntax->color;
		continue;
	    }

	    for (e = tmpsyntax->extensions; e != NULL; e = e->next) {
		bool not_compiled = (e->ext == NULL);

		/* e->ext_regex has already been checked for validity
		 * elsewhere.  Compile its specified regex if we haven't
		 * already. */
		if (not_compiled) {
		    e->ext = (regex_t *)nmalloc(sizeof(regex_t));
		    regcomp(e->ext, fixbounds(e->ext_regex), REG_EXTENDED);
		}

		/* Set colorstrings if we match the extension regex. */
		if (regexec(e->ext, fullname, 0, NULL, 0) == 0) {
		    openfile->syntax = tmpsyntax;
		    openfile->colorstrings = tmpsyntax->color;
		    break;
		}

		/* Decompile e->ext_regex's specified regex if we aren't
		 * going to use it. */
		if (not_compiled)
		    nfreeregex(&e->ext);
	    }
	}

	free(joinednames);
	free(fullname);

	/* Check the headerline if the extension didn't match anything. */
	if (openfile->colorstrings == NULL) {
#ifdef DEBUG
	    fprintf(stderr, "No result from file extension, trying headerline...\n");
#endif
	    for (tmpsyntax = syntaxes; tmpsyntax != NULL;
		tmpsyntax = tmpsyntax->next) {

		for (e = tmpsyntax->headers; e != NULL; e = e->next) {
		    bool not_compiled = (e->ext == NULL);

		    if (not_compiled) {
			e->ext = (regex_t *)nmalloc(sizeof(regex_t));
			regcomp(e->ext, fixbounds(e->ext_regex), REG_EXTENDED);
		    }
#ifdef DEBUG
		    fprintf(stderr, "Comparing header regex \"%s\" to fileage \"%s\"...\n",
				    e->ext_regex, openfile->fileage->data);
#endif
		    /* Set colorstrings if we match the header-line regex. */
		    if (regexec(e->ext, openfile->fileage->data, 0, NULL, 0) == 0) {
			openfile->syntax = tmpsyntax;
			openfile->colorstrings = tmpsyntax->color;
			break;
		    }

		    if (not_compiled)
			nfreeregex(&e->ext);
		}
	    }
	}

#ifdef HAVE_LIBMAGIC
	/* Check magic if we don't have an answer yet. */
	if (openfile->colorstrings == NULL) {
	    struct stat fileinfo;
	    magic_t cookie = NULL;
	    const char *magicstring = NULL;
#ifdef DEBUG
	    fprintf(stderr, "No result from headerline either, trying libmagic...\n");
#endif
	    if (stat(openfile->filename, &fileinfo) == 0) {
		/* Open the magic database and get a diagnosis of the file. */
		cookie = magic_open(MAGIC_SYMLINK |
#ifdef DEBUG
				    MAGIC_DEBUG | MAGIC_CHECK |
#endif
				    MAGIC_ERROR);
		if (cookie == NULL || magic_load(cookie, NULL) < 0)
		    statusbar(_("magic_load() failed: %s"), strerror(errno));
		else {
		    magicstring = magic_file(cookie, openfile->filename);
		    if (magicstring == NULL) {
			statusbar(_("magic_file(%s) failed: %s"),
					openfile->filename, magic_error(cookie));
		    }
#ifdef DEBUG
		    fprintf(stderr, "Returned magic string is: %s\n", magicstring);
#endif
		}
	    }

	    /* Now try and find a syntax that matches the magicstring. */
	    for (tmpsyntax = syntaxes; tmpsyntax != NULL;
		tmpsyntax = tmpsyntax->next) {

		for (e = tmpsyntax->magics; e != NULL; e = e->next) {
		    bool not_compiled = (e->ext == NULL);

		    if (not_compiled) {
			e->ext = (regex_t *)nmalloc(sizeof(regex_t));
			regcomp(e->ext, fixbounds(e->ext_regex), REG_EXTENDED);
		    }
#ifdef DEBUG
		    fprintf(stderr, "Matching regex \"%s\" against \"%s\"\n", e->ext_regex, magicstring);
#endif
		    /* Set colorstrings if we match the magic-string regex. */
		    if (magicstring && regexec(e->ext, magicstring, 0, NULL, 0) == 0) {
			openfile->syntax = tmpsyntax;
			openfile->colorstrings = tmpsyntax->color;
			break;
		    }

		    if (not_compiled)
			nfreeregex(&e->ext);
		}
		if (openfile->syntax != NULL)
		    break;
	    }
	    if (stat(openfile->filename, &fileinfo) == 0)
		magic_close(cookie);
	}
#endif /* HAVE_LIBMAGIC */
    }

    /* If we didn't find any syntax yet, and we do have a default one,
     * use it. */
    if (openfile->colorstrings == NULL && defcolor != NULL) {
	openfile->syntax = defsyntax;
	openfile->colorstrings = defcolor;
    }

    for (tmpcolor = openfile->colorstrings; tmpcolor != NULL;
	tmpcolor = tmpcolor->next) {
	/* tmpcolor->start_regex and tmpcolor->end_regex have already
	 * been checked for validity elsewhere.  Compile their specified
	 * regexes if we haven't already. */
	if (tmpcolor->start == NULL) {
	    tmpcolor->start = (regex_t *)nmalloc(sizeof(regex_t));
	    regcomp(tmpcolor->start, fixbounds(tmpcolor->start_regex),
		REG_EXTENDED | (tmpcolor->icase ? REG_ICASE : 0));
	}

	if (tmpcolor->end_regex != NULL && tmpcolor->end == NULL) {
	    tmpcolor->end = (regex_t *)nmalloc(sizeof(regex_t));
	    regcomp(tmpcolor->end, fixbounds(tmpcolor->end_regex),
		REG_EXTENDED | (tmpcolor->icase ? REG_ICASE : 0));
	}
    }
}

/* Reset the multiline coloring cache for one specific regex (given by
 * index) for lines that need reevaluation. */
void reset_multis_for_id(filestruct *fileptr, int index)
{
    filestruct *row;

    /* Reset the cache of earlier lines, as far back as needed. */
    for (row = fileptr->prev; row != NULL; row = row->prev) {
	alloc_multidata_if_needed(row);
	if (row->multidata[index] == CNONE)
	    break;
	row->multidata[index] = -1;
    }
    for (; row != NULL; row = row->prev) {
	alloc_multidata_if_needed(row);
	if (row->multidata[index] != CNONE)
	    break;
	row->multidata[index] = -1;
    }

    /* Reset the cache of the current line. */
    fileptr->multidata[index] = -1;

    /* Reset the cache of later lines, as far ahead as needed. */
    for (row = fileptr->next; row != NULL; row = row->next) {
	alloc_multidata_if_needed(row);
	if (row->multidata[index] == CNONE)
	    break;
	row->multidata[index] = -1;
    }
    for (; row != NULL; row = row->next) {
	alloc_multidata_if_needed(row);
	if (row->multidata[index] != CNONE)
	    break;
	row->multidata[index] = -1;
    }

    edit_refresh_needed = TRUE;
}

/* Reset multi-line strings around the filestruct fileptr, trying to be
 * smart about stopping.  Bool force means: reset everything regardless,
 * useful when we don't know how much screen state has changed. */
void reset_multis(filestruct *fileptr, bool force)
{
    int nobegin, noend;
    regmatch_t startmatch, endmatch;
    const colortype *tmpcolor = openfile->colorstrings;

    /* If there is no syntax or no multiline regex, there is nothing to do. */
    if (openfile->syntax == NULL || openfile->syntax->nmultis == 0)
	return;

    for (; tmpcolor != NULL; tmpcolor = tmpcolor->next) {
	/* If it's not a multi-line regex, amscray. */
	if (tmpcolor->end == NULL)
	    continue;

	alloc_multidata_if_needed(fileptr);

	if (force == FALSE) {
	    /* Check whether the multidata still matches the current situation. */
	    nobegin = regexec(tmpcolor->start, fileptr->data, 1, &startmatch, 0);
	    noend = regexec(tmpcolor->end, fileptr->data, 1, &endmatch, 0);
	    if ((fileptr->multidata[tmpcolor->id] == CWHOLELINE ||
			fileptr->multidata[tmpcolor->id] == CNONE) &&
			nobegin && noend)
		continue;
	    else if (fileptr->multidata[tmpcolor->id] == CSTARTENDHERE &&
			!nobegin && !noend && startmatch.rm_so < endmatch.rm_so)
		continue;
	    else if (fileptr->multidata[tmpcolor->id] == CBEGINBEFORE &&
			nobegin && !noend)
		continue;
	    else if (fileptr->multidata[tmpcolor->id] == CENDAFTER &&
			!nobegin && noend)
		continue;
	}

	/* If we got here, things have changed. */
	reset_multis_for_id(fileptr, tmpcolor->id);
    }
}

/* Allocate (for one line) the cache space for multiline color regexes. */
void alloc_multidata_if_needed(filestruct *fileptr)
{
    int i;

    if (fileptr->multidata == NULL) {
	fileptr->multidata = (short *)nmalloc(openfile->syntax->nmultis * sizeof(short));

	for (i = 0; i < openfile->syntax->nmultis; i++)
	    fileptr->multidata[i] = -1;
    }
}

/* Poll the keyboard every second to see if the user starts typing. */
bool key_was_pressed(void)
{
    static time_t last_time = 0;

    if (time(NULL) != last_time) {
	last_time = time(NULL);
	return (wgetch(edit) != ERR);
    } else
	return FALSE;
}

/* Precalculate the multi-line start and end regex info so we can
 * speed up rendering (with any hope at all...). */
void precalc_multicolorinfo(void)
{
    const colortype *tmpcolor = openfile->colorstrings;
    regmatch_t startmatch, endmatch;
    filestruct *fileptr, *endptr;

    if (openfile->colorstrings == NULL || ISSET(NO_COLOR_SYNTAX))
	return;

#ifdef DEBUG
    fprintf(stderr, "Entering precalculation of multiline color info\n");
#endif
    /* Let us get keypresses to see if the user is trying to start
     * editing.  Later we may want to throw up a statusbar message
     * before starting this if it takes too long to do this routine.
     * For now silently abort if they hit a key. */
    nodelay(edit, TRUE);

    for (; tmpcolor != NULL; tmpcolor = tmpcolor->next) {
	/* If this is not a multi-line regex, skip it. */
	if (tmpcolor->end == NULL)
	    continue;
#ifdef DEBUG
	fprintf(stderr, "Starting work on color id %d\n", tmpcolor->id);
#endif

	for (fileptr = openfile->fileage; fileptr != NULL; fileptr = fileptr->next) {
	    int startx = 0, nostart = 0;

	    if (key_was_pressed())
		goto precalc_cleanup;
#ifdef DEBUG
	    fprintf(stderr, "working on lineno %ld... ", (long)fileptr->lineno);
#endif
	    alloc_multidata_if_needed(fileptr);

	    while ((nostart = regexec(tmpcolor->start, &fileptr->data[startx],
			1, &startmatch, (startx == 0) ? 0 : REG_NOTBOL)) == 0) {
		/* Look for an end, and start marking how many lines are
		 * encompassed, which should speed up rendering later. */
		startx += startmatch.rm_eo;
#ifdef DEBUG
		fprintf(stderr, "start found at pos %lu... ", (unsigned long)startx);
#endif
		/* Look first on this line for an end. */
		if (regexec(tmpcolor->end, &fileptr->data[startx], 1,
			&endmatch, (startx == 0) ? 0 : REG_NOTBOL) == 0) {
		    startx += endmatch.rm_eo;
		    /* Step ahead when both start and end are mere anchors. */
		    if (startmatch.rm_so == startmatch.rm_eo &&
				endmatch.rm_so == endmatch.rm_eo)
			startx += 1;
		    fileptr->multidata[tmpcolor->id] = CSTARTENDHERE;
#ifdef DEBUG
		    fprintf(stderr, "end found on this line\n");
#endif
		    continue;
		}

		/* Nice, we didn't find the end regex on this line.  Let's start looking for it. */
		for (endptr = fileptr->next; endptr != NULL; endptr = endptr->next) {
#ifdef DEBUG
		    fprintf(stderr, "\nadvancing to line %ld to find end... ", (long)endptr->lineno);
#endif
		    /* Check for interrupting keyboard input again. */
		    if (key_was_pressed())
			goto precalc_cleanup;

		    if (regexec(tmpcolor->end, endptr->data, 1, &endmatch, 0) == 0)
			break;
		}

		if (endptr == NULL) {
#ifdef DEBUG
		    fprintf(stderr, "no end found, breaking out\n");
#endif
		    break;
		}
#ifdef DEBUG
		fprintf(stderr, "end found\n");
#endif
		/* We found it, we found it, la la la la la.  Mark all
		 * the lines in between and the end properly. */
		fileptr->multidata[tmpcolor->id] = CENDAFTER;
#ifdef DEBUG
		fprintf(stderr, "marking line %ld as CENDAFTER\n", (long)fileptr->lineno);
#endif
		for (fileptr = fileptr->next; fileptr != endptr; fileptr = fileptr->next) {
		    alloc_multidata_if_needed(fileptr);
		    fileptr->multidata[tmpcolor->id] = CWHOLELINE;
#ifdef DEBUG
		    fprintf(stderr, "marking intermediary line %ld as CWHOLELINE\n", (long)fileptr->lineno);
#endif
		}

		alloc_multidata_if_needed(endptr);
		fileptr->multidata[tmpcolor->id] = CBEGINBEFORE;
#ifdef DEBUG
		fprintf(stderr, "marking line %ld as CBEGINBEFORE\n", (long)fileptr->lineno);
#endif
		/* Skip to the end point of the match. */
		startx = endmatch.rm_eo;
#ifdef DEBUG
		fprintf(stderr, "jumping to line %ld pos %lu to continue\n", (long)fileptr->lineno, (unsigned long)startx);
#endif
	    }

	    if (nostart && startx == 0) {
#ifdef DEBUG
		fprintf(stderr, "no match\n");
#endif
		fileptr->multidata[tmpcolor->id] = CNONE;
		continue;
	    }
	}
    }
precalc_cleanup:
    nodelay(edit, FALSE);
}

#endif /* !DISABLE_COLOR */
