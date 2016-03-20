/* $Id: search.c 5653 2016-02-20 12:16:43Z bens $ */
/**************************************************************************
 *   search.c                                                             *
 *                                                                        *
 *   Copyright (C) 1999, 2000, 2001, 2002, 2003, 2004, 2005, 2006, 2007,  *
 *   2008, 2009, 2010, 2011, 2013, 2014 Free Software Foundation, Inc.    *
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

#include <string.h>
#include <stdio.h>
#include <unistd.h>
#include <ctype.h>
#include <errno.h>
#include <time.h>

static bool search_last_line = FALSE;
	/* Have we gone past the last line while searching? */
#ifndef DISABLE_HISTORIES
static bool history_changed = FALSE;
	/* Have any of the history lists changed? */
#endif
#ifdef HAVE_REGEX_H
static bool regexp_compiled = FALSE;
	/* Have we compiled any regular expressions? */

/* Compile the regular expression regexp to see if it's valid.  Return
 * TRUE if it is, or FALSE otherwise. */
bool regexp_init(const char *regexp)
{
    int rc;

    assert(!regexp_compiled);

    rc = regcomp(&search_regexp, regexp, REG_EXTENDED
#ifndef NANO_TINY
	| (ISSET(CASE_SENSITIVE) ? 0 : REG_ICASE)
#endif
	);

    if (rc != 0) {
	size_t len = regerror(rc, &search_regexp, NULL, 0);
	char *str = charalloc(len);

	regerror(rc, &search_regexp, str, len);
	statusbar(_("Bad regex \"%s\": %s"), regexp, str);
	free(str);

	return FALSE;
    }

    regexp_compiled = TRUE;

    return TRUE;
}

/* Decompile the compiled regular expression we used in the last
 * search, if any. */
void regexp_cleanup(void)
{
    if (regexp_compiled) {
	regexp_compiled = FALSE;
	regfree(&search_regexp);
    }
}
#endif

/* Indicate on the statusbar that the string at str was not found by the
 * last search. */
void not_found_msg(const char *str)
{
    char *disp;
    int numchars;

    assert(str != NULL);

    disp = display_string(str, 0, (COLS / 2) + 1, FALSE);
    numchars = actual_x(disp, mbstrnlen(disp, COLS / 2));

    statusbar(_("\"%.*s%s\" not found"), numchars, disp,
	(disp[numchars] == '\0') ? "" : "...");

    free(disp);
}

/* Abort the current search or replace.  Clean up by displaying the main
 * shortcut list, updating the screen if the mark was on before, and
 * decompiling the compiled regular expression we used in the last
 * search, if any. */
void search_replace_abort(void)
{
    display_main_list();
    focusing = FALSE;
#ifndef NANO_TINY
    if (openfile->mark_set)
	edit_refresh();
#endif
#ifdef HAVE_REGEX_H
    regexp_cleanup();
#endif
}

/* Set up the system variables for a search or replace.  If use_answer
 * is TRUE, only set backupstring to answer.  Return -2 to run the
 * opposite program (search -> replace, replace -> search), return -1 if
 * the search should be canceled (due to Cancel, a blank search string,
 * Go to Line, or a failed regcomp()), return 0 on success, and return 1
 * on rerun calling program.
 *
 * replacing is TRUE if we call from do_replace(), and FALSE if called
 * from do_search(). */
int search_init(bool replacing, bool use_answer)
{
    int i = 0;
    char *buf;
    static char *backupstring = NULL;
	/* The search string we'll be using. */

    /* If backupstring doesn't exist, initialize it to "". */
    if (backupstring == NULL)
	backupstring = mallocstrcpy(NULL, "");

    /* If use_answer is TRUE, set backupstring to answer and get out. */
    if (use_answer) {
	backupstring = mallocstrcpy(backupstring, answer);
	return 0;
    }

    /* We display the search prompt below.  If the user types a partial
     * search string and then Replace or a toggle, we will return to
     * do_search() or do_replace() and be called again.  In that case,
     * we should put the same search string back up. */

    focusing = TRUE;

    if (last_search[0] != '\0') {
	char *disp = display_string(last_search, 0, COLS / 3, FALSE);

	buf = charalloc(strlen(disp) + 7);
	/* We use (COLS / 3) here because we need to see more on the line. */
	sprintf(buf, " [%s%s]", disp,
		(strlenpt(last_search) > COLS / 3) ? "..." : "");
	free(disp);
    } else
	buf = mallocstrcpy(NULL, "");

    /* This is now one simple call.  It just does a lot. */
    i = do_prompt(FALSE,
#ifndef DISABLE_TABCOMP
	TRUE,
#endif
	replacing ? MREPLACE : MWHEREIS, backupstring,
#ifndef DISABLE_HISTORIES
	&search_history,
#endif
	/* TRANSLATORS: This is the main search prompt. */
	edit_refresh, "%s%s%s%s%s%s", _("Search"),
#ifndef NANO_TINY
	/* TRANSLATORS: The next three strings are modifiers of the search prompt. */
	ISSET(CASE_SENSITIVE) ? _(" [Case Sensitive]") :
#endif
	"",
#ifdef HAVE_REGEX_H
	ISSET(USE_REGEXP) ? _(" [Regexp]") :
#endif
	"",
#ifndef NANO_TINY
	ISSET(BACKWARDS_SEARCH) ? _(" [Backwards]") :
#endif
	"", replacing ?
#ifndef NANO_TINY
	/* TRANSLATORS: The next two strings are modifiers of the search prompt. */
	openfile->mark_set ? _(" (to replace) in selection") :
#endif
	_(" (to replace)") : "", buf);

    /* Release buf now that we don't need it anymore. */
    free(buf);

    free(backupstring);
    backupstring = NULL;

    /* Cancel any search, or just return with no previous search. */
    if (i == -1 || (i < 0 && *last_search == '\0') || (!replacing &&
	i == 0 && *answer == '\0')) {
	statusbar(_("Cancelled"));
	return -1;
    } else {
	functionptrtype func = func_from_key(&i);

	if (i == -2 || i == 0 ) {
#ifdef HAVE_REGEX_H
		/* Use last_search if answer is an empty string, or
		 * answer if it isn't. */
		if (ISSET(USE_REGEXP) && !regexp_init((i == -2) ?
			last_search : answer))
		    return -1;
#endif
		;
#ifndef NANO_TINY
	} else if (func == case_sens_void) {
		TOGGLE(CASE_SENSITIVE);
		backupstring = mallocstrcpy(backupstring, answer);
		return 1;
	} else if (func == backwards_void) {
		TOGGLE(BACKWARDS_SEARCH);
		backupstring = mallocstrcpy(backupstring, answer);
		return 1;
#endif
#ifdef HAVE_REGEX_H
	} else if (func == regexp_void) {
		TOGGLE(USE_REGEXP);
		backupstring = mallocstrcpy(backupstring, answer);
		return 1;
#endif
	} else if (func == do_replace || func == flip_replace_void) {
		backupstring = mallocstrcpy(backupstring, answer);
		return -2;	/* Call the opposite search function. */
	} else if (func == do_gotolinecolumn_void) {
		do_gotolinecolumn(openfile->current->lineno,
			openfile->placewewant + 1, TRUE, TRUE);
				/* Put answer up on the statusbar and
				 * fall through. */
		return 3;
	} else {
		return -1;
	}
    }

    return 0;
}

/* Look for needle, starting at (current, current_x).  begin is the line
 * where we first started searching, at column begin_x.  The return
 * value specifies whether we found anything.  If we did, set needle_len
 * to the length of the string we found if it isn't NULL. */
bool findnextstr(
#ifndef DISABLE_SPELLER
	bool whole_word_only,
#endif
	const filestruct *begin, size_t begin_x,
	const char *needle, size_t *needle_len)
{
    size_t found_len;
	/* The length of the match we find. */
    size_t current_x_find = 0;
	/* The location in the current line of the match we find. */
    ssize_t current_y_find = openfile->current_y;
    filestruct *fileptr = openfile->current;
    const char *rev_start = fileptr->data, *found = NULL;
    time_t lastkbcheck = time(NULL);

    /* rev_start might end up 1 character before the start or after the
     * end of the line.  This won't be a problem because strstrwrapper()
     * will return immediately and say that no match was found, and
     * rev_start will be properly set when the search continues on the
     * previous or next line. */
    rev_start +=
#ifndef NANO_TINY
	ISSET(BACKWARDS_SEARCH) ?
	((openfile->current_x == 0) ? -1 : move_mbleft(fileptr->data, openfile->current_x)) :
#endif
	move_mbright(fileptr->data, openfile->current_x);

    /* Look for needle in the current line we're searching. */
    enable_nodelay();
    while (TRUE) {
	if (time(NULL) - lastkbcheck > 1) {
	    int input = parse_kbinput(edit);

	    lastkbcheck = time(NULL);

	    if (input && func_from_key(&input) == do_cancel) {
		statusbar(_("Cancelled"));
		return FALSE;
	    }
	}

	found = strstrwrapper(fileptr->data, needle, rev_start);

	/* We've found a potential match. */
	if (found != NULL) {
#ifndef DISABLE_SPELLER
	    bool found_whole = FALSE;
		/* Is this potential match a whole word? */
#endif

	    /* Set found_len to the length of the potential match. */
	    found_len =
#ifdef HAVE_REGEX_H
		ISSET(USE_REGEXP) ?
		regmatches[0].rm_eo - regmatches[0].rm_so :
#endif
		strlen(needle);

#ifndef DISABLE_SPELLER
	    /* If we're searching for whole words, see if this potential
	     * match is a whole word. */
	    if (whole_word_only) {
		char *word = mallocstrncpy(NULL, found, found_len + 1);
		word[found_len] = '\0';

		found_whole = is_whole_word(found - fileptr->data,
			fileptr->data, word);
		free(word);
	    }

	    /* If we're searching for whole words and this potential
	     * match isn't a whole word, continue searching. */
	    if (!whole_word_only || found_whole)
#endif
		break;
	}

	if (search_last_line) {
	    /* We've finished processing the file, so get out. */
	    not_found_msg(needle);
	    disable_nodelay();
	    return FALSE;
	}

	/* Move to the previous or next line in the file. */
#ifndef NANO_TINY
	if (ISSET(BACKWARDS_SEARCH)) {
	    fileptr = fileptr->prev;
	    current_y_find--;
	} else {
#endif
	    fileptr = fileptr->next;
	    current_y_find++;
#ifndef NANO_TINY
	}
#endif

	if (fileptr == NULL) {
	    /* We've reached the start or end of the buffer, so wrap around. */
#ifndef NANO_TINY
	    if (ISSET(BACKWARDS_SEARCH)) {
		fileptr = openfile->filebot;
		current_y_find = editwinrows - 1;
	    } else {
#endif
		fileptr = openfile->fileage;
		current_y_find = 0;
#ifndef NANO_TINY
	    }
#endif
	    statusbar(_("Search Wrapped"));
	}

	if (fileptr == begin)
	    /* We've reached the original starting line. */
	    search_last_line = TRUE;

	rev_start = fileptr->data;
#ifndef NANO_TINY
	if (ISSET(BACKWARDS_SEARCH))
	    rev_start += strlen(fileptr->data);
#endif
    }

    /* We found an instance. */
    current_x_find = found - fileptr->data;

    /* Ensure we haven't wrapped around again! */
    if (search_last_line &&
#ifndef NANO_TINY
	((!ISSET(BACKWARDS_SEARCH) && current_x_find > begin_x) ||
	(ISSET(BACKWARDS_SEARCH) && current_x_find < begin_x))
#else
	current_x_find > begin_x
#endif
	) {
	not_found_msg(needle);
	disable_nodelay();
	return FALSE;
    }

    disable_nodelay();
    /* We've definitely found something. */
    openfile->current = fileptr;
    openfile->current_x = current_x_find;
    openfile->placewewant = xplustabs();
    openfile->current_y = current_y_find;

    /* needle_len holds the length of needle. */
    if (needle_len != NULL)
	*needle_len = found_len;

    return TRUE;
}

/* Clear the flag indicating that a search reached the last line of the
 * file.  We need to do this just before a new search. */
void findnextstr_wrap_reset(void)
{
    search_last_line = FALSE;
}

/* Search for a string. */
void do_search(void)
{
    filestruct *fileptr = openfile->current;
    size_t fileptr_x = openfile->current_x;
    size_t pww_save = openfile->placewewant;
    int i;
    bool didfind;

    i = search_init(FALSE, FALSE);

    if (i == -1)
	/* Cancel, Go to Line, blank search string, or regcomp() failed. */
	search_replace_abort();
    else if (i == -2)
	/* Replace. */
	do_replace();
#if !defined(NANO_TINY) || defined(HAVE_REGEX_H)
    else if (i == 1)
	/* Case Sensitive, Backwards, or Regexp search toggle. */
	do_search();
#endif

    if (i != 0)
	return;

    /* If answer is now "", copy last_search into answer. */
    if (*answer == '\0')
	answer = mallocstrcpy(answer, last_search);
    else
	last_search = mallocstrcpy(last_search, answer);

#ifndef DISABLE_HISTORIES
    /* If answer is not "", add this search string to the search history
     * list. */
    if (answer[0] != '\0')
	update_history(&search_history, answer);
#endif

    findnextstr_wrap_reset();
    didfind = findnextstr(
#ifndef DISABLE_SPELLER
	FALSE,
#endif
	openfile->current, openfile->current_x, answer, NULL);

    /* If we found something, and we're back at the exact same spot where
     * we started searching, then this is the only occurrence. */
    if (didfind && fileptr == openfile->current &&
		fileptr_x == openfile->current_x)
	statusbar(_("This is the only occurrence"));

    openfile->placewewant = xplustabs();
    edit_redraw(fileptr, pww_save);
    search_replace_abort();
}

#ifndef NANO_TINY
/* Search in the backward direction for the next occurrence. */
void do_findprevious(void)
{
    if ISSET(BACKWARDS_SEARCH)
	do_research();
    else {
	SET(BACKWARDS_SEARCH);
	do_research();
	UNSET(BACKWARDS_SEARCH);
    }
}

/* Search in the forward direction for the next occurrence. */
void do_findnext(void)
{
    if ISSET(BACKWARDS_SEARCH) {
	UNSET(BACKWARDS_SEARCH);
	do_research();
	SET(BACKWARDS_SEARCH);
    } else
	do_research();
}
#endif

#if !defined(NANO_TINY) || !defined(DISABLE_BROWSER)
/* Search for the last string without prompting. */
void do_research(void)
{
    filestruct *fileptr = openfile->current;
    size_t fileptr_x = openfile->current_x;
    size_t pww_save = openfile->placewewant;
    bool didfind;

    focusing = TRUE;

#ifndef DISABLE_HISTORIES
    /* If nothing was searched for yet during this run of nano, but
     * there is a search history, take the most recent item. */
    if (last_search[0] == '\0' && searchbot->prev != NULL)
	last_search = mallocstrcpy(last_search, searchbot->prev->data);
#endif

    if (last_search[0] == '\0')
	statusbar(_("No current search pattern"));
    else {
#ifdef HAVE_REGEX_H
	/* Since answer is "", use last_search! */
	if (ISSET(USE_REGEXP) && !regexp_init(last_search))
	    return;
#endif

	findnextstr_wrap_reset();
	didfind = findnextstr(
#ifndef DISABLE_SPELLER
		FALSE,
#endif
		openfile->current, openfile->current_x, last_search, NULL);

	/* If we found something, and we're back at the exact same spot
	 * where we started searching, then this is the only occurrence. */
	if (didfind && fileptr == openfile->current &&
		fileptr_x == openfile->current_x && didfind)
	    statusbar(_("This is the only occurrence"));
    }

    openfile->placewewant = xplustabs();
    edit_redraw(fileptr, pww_save);
    search_replace_abort();
}
#endif /* !NANO_TINY */

#ifdef HAVE_REGEX_H
/* Calculate the size of the replacement text, taking possible
 * subexpressions \1 to \9 into account.  Return the replacement
 * text in the passed string only when create is TRUE. */
int replace_regexp(char *string, bool create)
{
    const char *c = last_replace;
    size_t replacement_size = 0;

    /* Iterate through the replacement text to handle subexpression
     * replacement using \1, \2, \3, etc. */
    while (*c != '\0') {
	int num = (*(c + 1) - '0');

	if (*c != '\\' || num < 1 || num > 9 || num >
		search_regexp.re_nsub) {
	    if (create)
		*string++ = *c;
	    c++;
	    replacement_size++;
	} else {
	    size_t i = regmatches[num].rm_eo - regmatches[num].rm_so;

	    /* Skip over the replacement expression. */
	    c += 2;

	    /* But add the length of the subexpression to new_size. */
	    replacement_size += i;

	    /* And if create is TRUE, append the result of the
	     * subexpression match to the new line. */
	    if (create) {
		strncpy(string, openfile->current->data +
			openfile->current_x + regmatches[num].rm_so, i);
		string += i;
	    }
	}
    }

    if (create)
	*string = '\0';

    return replacement_size;
}
#endif /* HAVE_REGEX_H */

/* Return a copy of the current line with one needle replaced. */
char *replace_line(const char *needle)
{
    char *copy;
    size_t match_len;
    size_t new_line_size = strlen(openfile->current->data) + 1;

    /* First adjust the size of the new line for the change. */
#ifdef HAVE_REGEX_H
    if (ISSET(USE_REGEXP)) {
	match_len = regmatches[0].rm_eo - regmatches[0].rm_so;
	new_line_size += replace_regexp(NULL, FALSE) - match_len;
    } else {
#endif
	match_len = strlen(needle);
	new_line_size += strlen(answer) - match_len;
#ifdef HAVE_REGEX_H
    }
#endif

    /* Create the buffer. */
    copy = charalloc(new_line_size);

    /* Copy the head of the original line. */
    strncpy(copy, openfile->current->data, openfile->current_x);

    /* Add the replacement text. */
#ifdef HAVE_REGEX_H
    if (ISSET(USE_REGEXP))
	replace_regexp(copy + openfile->current_x, TRUE);
    else
#endif
	strcpy(copy + openfile->current_x, answer);

    assert(openfile->current_x + match_len <= strlen(openfile->current->data));

    /* Copy the tail of the original line. */
    strcat(copy, openfile->current->data + openfile->current_x + match_len);

    return copy;
}

/* Step through each replace word and prompt user before replacing.
 * Parameters real_current and real_current_x are needed in order to
 * allow the cursor position to be updated when a word before the cursor
 * is replaced by a shorter word.
 *
 * needle is the string to seek.  We replace it with answer.  Return -1
 * if needle isn't found, else the number of replacements performed.  If
 * canceled isn't NULL, set it to TRUE if we canceled. */
ssize_t do_replace_loop(
#ifndef DISABLE_SPELLER
	bool whole_word_only,
#endif
	bool *canceled, const filestruct *real_current, size_t
	*real_current_x, const char *needle)
{
    ssize_t numreplaced = -1;
    size_t match_len;
    bool replaceall = FALSE;
#ifndef NANO_TINY
    bool old_mark_set = openfile->mark_set;
    filestruct *top, *bot;
    size_t top_x, bot_x;
    bool right_side_up = FALSE;
	/* TRUE if (mark_begin, mark_begin_x) is the top of the mark,
	 * FALSE if (current, current_x) is. */

    if (old_mark_set) {
	/* If the mark is on, frame the region, and turn the mark off. */
	mark_order((const filestruct **)&top, &top_x,
	    (const filestruct **)&bot, &bot_x, &right_side_up);
	openfile->mark_set = FALSE;

	/* Start either at the top or the bottom of the marked region. */
	if (!ISSET(BACKWARDS_SEARCH)) {
	    openfile->current = top;
	    openfile->current_x = (top_x == 0 ? 0 : top_x - 1);
	} else {
	    openfile->current = bot;
	    openfile->current_x = bot_x;
	}
    }
#endif /* !NANO_TINY */

    if (canceled != NULL)
	*canceled = FALSE;

    findnextstr_wrap_reset();
    while (findnextstr(
#ifndef DISABLE_SPELLER
	whole_word_only,
#endif
	real_current, *real_current_x, needle, &match_len)) {
	int i = 0;

#ifndef NANO_TINY
	if (old_mark_set) {
	    /* When we've found an occurrence outside of the marked region,
	     * stop the fanfare. */
	    if (openfile->current->lineno > bot->lineno ||
		openfile->current->lineno < top->lineno ||
		(openfile->current == bot && openfile->current_x > bot_x) ||
		(openfile->current == top && openfile->current_x < top_x))
		break;
	}
#endif

	/* Indicate that we found the search string. */
	if (numreplaced == -1)
	    numreplaced = 0;

	if (!replaceall) {
	    size_t xpt = xplustabs();
	    char *exp_word = display_string(openfile->current->data,
		xpt, strnlenpt(openfile->current->data,
		openfile->current_x + match_len) - xpt, FALSE);

	    /* Refresh the edit window, scrolling it if necessary. */
	    edit_refresh();

	    /* Don't show cursor, to not distract from highlighted match. */
	    curs_set(0);

	    do_replace_highlight(TRUE, exp_word);

	    /* TRANSLATORS: This is a prompt. */
	    i = do_yesno_prompt(TRUE, _("Replace this instance?"));

	    do_replace_highlight(FALSE, exp_word);

	    free(exp_word);

	    if (i == -1) {	/* We canceled the replace. */
		if (canceled != NULL)
		    *canceled = TRUE;
		break;
	    }
	}

	if (i > 0 || replaceall) {	/* Yes, replace it!!!! */
	    char *copy;
	    size_t length_change;

#ifndef NANO_TINY
	    add_undo(REPLACE);
#endif
	    if (i == 2)
		replaceall = TRUE;

	    copy = replace_line(needle);

	    length_change = strlen(copy) - strlen(openfile->current->data);

#ifndef NANO_TINY
	    /* If the mark was on and it was located after the cursor,
	     * then adjust its x position for any text length changes. */
	    if (old_mark_set && !right_side_up) {
		if (openfile->current == openfile->mark_begin &&
			openfile->mark_begin_x > openfile->current_x) {
		    if (openfile->mark_begin_x < openfile->current_x + match_len)
			openfile->mark_begin_x = openfile->current_x;
		    else
			openfile->mark_begin_x += length_change;
		    bot_x = openfile->mark_begin_x;
		}
	    }

	    /* If the mark was not on or it was before the cursor, then
	     * adjust the cursor's x position for any text length changes. */
	    if (!old_mark_set || right_side_up) {
#endif
		if (openfile->current == real_current &&
			openfile->current_x <= *real_current_x) {
		    if (*real_current_x < openfile->current_x + match_len)
			*real_current_x = openfile->current_x + match_len;
		    *real_current_x += length_change;
#ifndef NANO_TINY
		    bot_x = *real_current_x;
		}
#endif
	    }

#ifdef HAVE_REGEX_H
	    /* Don't find the same zero-length match again. */
	    if (match_len == 0)
		match_len++;
#endif

	    /* Set the cursor at the last character of the replacement
	     * text, so searching will resume after the replacement
	     * text.  Note that current_x might be set to (size_t)-1
	     * here. */
#ifndef NANO_TINY
	    if (!ISSET(BACKWARDS_SEARCH))
#endif
		openfile->current_x += match_len + length_change - 1;

	    /* Update the file size, and put the changed line into place. */
	    openfile->totsize += mbstrlen(copy) - mbstrlen(openfile->current->data);
	    free(openfile->current->data);
	    openfile->current->data = copy;

#ifndef DISABLE_COLOR
	    /* Reset the precalculated multiline-regex hints only when
	     * the first replacement has been made. */
	    if (numreplaced == 0)
		reset_multis(openfile->current, TRUE);
#endif

	    if (!replaceall) {
#ifndef DISABLE_COLOR
		/* If color syntaxes are available and turned on, we
		 * need to call edit_refresh(). */
		if (openfile->colorstrings != NULL && !ISSET(NO_COLOR_SYNTAX))
		    edit_refresh();
		else
#endif
		    update_line(openfile->current, openfile->current_x);
	    }

	    set_modified();
	    numreplaced++;
	}
    }

    if (numreplaced == -1)
	not_found_msg(needle);

#ifndef NANO_TINY
    if (old_mark_set)
	openfile->mark_set = TRUE;
#endif

    /* If the NO_NEWLINES flag isn't set, and text has been added to the
     * magicline, make a new magicline. */
    if (!ISSET(NO_NEWLINES) && openfile->filebot->data[0] != '\0')
	new_magicline();

    return numreplaced;
}

/* Replace a string. */
void do_replace(void)
{
    filestruct *edittop_save, *begin;
    size_t begin_x, pww_save;
    ssize_t numreplaced;
    int i;

    if (ISSET(VIEW_MODE)) {
	print_view_warning();
	search_replace_abort();
	return;
    }

    i = search_init(TRUE, FALSE);
    if (i == -1) {
	/* Cancel, Go to Line, blank search string, or regcomp() failed. */
	search_replace_abort();
	return;
    } else if (i == -2) {
	/* No Replace. */
	do_search();
	return;
    } else if (i == 1)
	/* Case Sensitive, Backwards, or Regexp search toggle. */
	do_replace();

    if (i != 0)
	return;

    /* If answer is not "", add answer to the search history list and
     * copy answer into last_search. */
    if (answer[0] != '\0') {
#ifndef DISABLE_HISTORIES
	update_history(&search_history, answer);
#endif
	last_search = mallocstrcpy(last_search, answer);
    }

    last_replace = mallocstrcpy(last_replace, "");

    i = do_prompt(FALSE,
#ifndef DISABLE_TABCOMP
	TRUE,
#endif
	MREPLACEWITH, last_replace,
#ifndef DISABLE_HISTORIES
	&replace_history,
#endif
	/* TRANSLATORS: This is a prompt. */
	edit_refresh, _("Replace with"));

#ifndef DISABLE_HISTORIES
    /* If the replace string is not "", add it to the replace history list. */
    if (i == 0)
	update_history(&replace_history, answer);
#endif

    if (i != 0 && i != -2) {
	if (i == -1) {		/* Cancel. */
	    if (last_replace[0] != '\0')
		answer = mallocstrcpy(answer, last_replace);
	    statusbar(_("Cancelled"));
	}
	search_replace_abort();
	return;
    }

    last_replace = mallocstrcpy(last_replace, answer);

    /* Save where we are. */
    edittop_save = openfile->edittop;
    begin = openfile->current;
    begin_x = openfile->current_x;
    pww_save = openfile->placewewant;

    numreplaced = do_replace_loop(
#ifndef DISABLE_SPELLER
	FALSE,
#endif
	NULL, begin, &begin_x, last_search);

    /* Restore where we were. */
    openfile->edittop = edittop_save;
    openfile->current = begin;
    openfile->current_x = begin_x;
    openfile->placewewant = pww_save;

    edit_refresh();

    if (numreplaced >= 0)
	statusbar(P_("Replaced %lu occurrence",
		"Replaced %lu occurrences", (unsigned long)numreplaced),
		(unsigned long)numreplaced);

    search_replace_abort();
}

/* Go to the specified line and x position. */
void goto_line_posx(ssize_t line, size_t pos_x)
{
    for (openfile->current = openfile->fileage; line > 1 &&
		openfile->current != openfile->filebot; line--)
	openfile->current = openfile->current->next;

    openfile->current_x = pos_x;
    openfile->placewewant = xplustabs();

    edit_refresh_needed = TRUE;
}

/* Go to the specified line and column, or ask for them if interactive
 * is TRUE.  In the latter case also update the screen afterwards.
 * Note that both the line and column number should be one-based. */
void do_gotolinecolumn(ssize_t line, ssize_t column, bool use_answer,
	bool interactive)
{
    if (interactive) {
	char *ans = mallocstrcpy(NULL, answer);
	functionptrtype func;

	/* Ask for the line and column. */
	int i = do_prompt(FALSE,
#ifndef DISABLE_TABCOMP
		TRUE,
#endif
		MGOTOLINE, use_answer ? ans : "",
#ifndef DISABLE_HISTORIES
		NULL,
#endif
		/* TRANSLATORS: This is a prompt. */
		edit_refresh, _("Enter line number, column number"));

	free(ans);

	/* Cancel, or Enter with blank string. */
	if (i < 0) {
	    statusbar(_("Cancelled"));
	    display_main_list();
	    return;
	}

	func = func_from_key(&i);

	if (func == gototext_void) {
	    /* Keep answer up on the statusbar. */
	    search_init(TRUE, TRUE);

	    do_search();
	    return;
	}

	/* Do a bounds check.  Display a warning on an out-of-bounds
	 * line or column number only if we hit Enter at the statusbar
	 * prompt. */
	if (!parse_line_column(answer, &line, &column) || line < 1 ||
		column < 1) {
	    if (i == 0)
		statusbar(_("Invalid line or column number"));
	    display_main_list();
	    return;
	}
    } else {
	if (line < 1)
	    line = openfile->current->lineno;

	if (column < 1)
	    column = openfile->placewewant + 1;
    }

    for (openfile->current = openfile->fileage; line > 1 &&
		openfile->current != openfile->filebot; line--)
	openfile->current = openfile->current->next;

    openfile->current_x = actual_x(openfile->current->data, column - 1);
    openfile->placewewant = column - 1;

    /* Put the top line of the edit window in range of the current line. */
    edit_update(CENTER);

    /* When in interactive mode, update the screen. */
    if (interactive) {
	edit_refresh();
	display_main_list();
    }
}

/* Go to the specified line and column, asking for them beforehand. */
void do_gotolinecolumn_void(void)
{
    do_gotolinecolumn(openfile->current->lineno,
	openfile->placewewant + 1, FALSE, TRUE);
}

#ifndef NANO_TINY
/* Search for a match to one of the two characters in bracket_set.  If
 * reverse is TRUE, search backwards for the leftmost bracket.
 * Otherwise, search forwards for the rightmost bracket.  Return TRUE if
 * we found a match, and FALSE otherwise. */
bool find_bracket_match(bool reverse, const char *bracket_set)
{
    filestruct *fileptr = openfile->current;
    const char *rev_start = NULL, *found = NULL;
    ssize_t current_y_find = openfile->current_y;

    assert(mbstrlen(bracket_set) == 2);

    /* rev_start might end up 1 character before the start or after the
     * end of the line.  This won't be a problem because we'll skip over
     * it below in that case, and rev_start will be properly set when
     * the search continues on the previous or next line. */
    rev_start = reverse ? fileptr->data + (openfile->current_x - 1) :
	fileptr->data + (openfile->current_x + 1);

    /* Look for either of the two characters in bracket_set.  rev_start
     * can be 1 character before the start or after the end of the line.
     * In either case, just act as though no match is found. */
    while (TRUE) {
	found = ((rev_start > fileptr->data && *(rev_start - 1) ==
		'\0') || rev_start < fileptr->data) ? NULL : (reverse ?
		mbrevstrpbrk(fileptr->data, bracket_set, rev_start) :
		mbstrpbrk(rev_start, bracket_set));

	if (found != NULL)
	    /* We've found a potential match. */
	    break;

	if (reverse) {
	    fileptr = fileptr->prev;
	    current_y_find--;
	} else {
	    fileptr = fileptr->next;
	    current_y_find++;
	}

	if (fileptr == NULL)
	    /* We've reached the start or end of the buffer, so get out. */
	    return FALSE;

	rev_start = fileptr->data;
	if (reverse)
	    rev_start += strlen(fileptr->data);
    }

    /* We've definitely found something. */
    openfile->current = fileptr;
    openfile->current_x = found - fileptr->data;
    openfile->placewewant = xplustabs();
    openfile->current_y = current_y_find;

    return TRUE;
}

/* Search for a match to the bracket at the current cursor position, if
 * there is one. */
void do_find_bracket(void)
{
    filestruct *current_save;
    size_t current_x_save, pww_save;
    const char *ch;
	/* The location in matchbrackets of the bracket at the current
	 * cursor position. */
    int ch_len;
	/* The length of ch in bytes. */
    const char *wanted_ch;
	/* The location in matchbrackets of the bracket complementing
	 * the bracket at the current cursor position. */
    int wanted_ch_len;
	/* The length of wanted_ch in bytes. */
    char *bracket_set;
	/* The pair of characters in ch and wanted_ch. */
    size_t i;
	/* Generic loop variable. */
    size_t matchhalf;
	/* The number of single-byte characters in one half of
	 * matchbrackets. */
    size_t mbmatchhalf;
	/* The number of multibyte characters in one half of
	 * matchbrackets. */
    size_t count = 1;
	/* The initial bracket count. */
    bool reverse;
	/* The direction we search. */
    char *found_ch;
	/* The character we find. */

    assert(mbstrlen(matchbrackets) % 2 == 0);

    ch = openfile->current->data + openfile->current_x;

    if ((ch = mbstrchr(matchbrackets, ch)) == NULL) {
	statusbar(_("Not a bracket"));
	return;
    }

    /* Save where we are. */
    current_save = openfile->current;
    current_x_save = openfile->current_x;
    pww_save = openfile->placewewant;

    /* If we're on an opening bracket, which must be in the first half
     * of matchbrackets, we want to search forwards for a closing
     * bracket.  If we're on a closing bracket, which must be in the
     * second half of matchbrackets, we want to search backwards for an
     * opening bracket. */
    matchhalf = 0;
    mbmatchhalf = mbstrlen(matchbrackets) / 2;

    for (i = 0; i < mbmatchhalf; i++)
	matchhalf += parse_mbchar(matchbrackets + matchhalf, NULL,
		NULL);

    reverse = ((ch - matchbrackets) >= matchhalf);

    /* If we're on an opening bracket, set wanted_ch to the character
     * that's matchhalf characters after ch.  If we're on a closing
     * bracket, set wanted_ch to the character that's matchhalf
     * characters before ch. */
    wanted_ch = ch;

    while (mbmatchhalf > 0) {
	if (reverse)
	    wanted_ch = matchbrackets + move_mbleft(matchbrackets,
		wanted_ch - matchbrackets);
	else
	    wanted_ch += move_mbright(wanted_ch, 0);

	mbmatchhalf--;
    }

    ch_len = parse_mbchar(ch, NULL, NULL);
    wanted_ch_len = parse_mbchar(wanted_ch, NULL, NULL);

    /* Fill bracket_set in with the values of ch and wanted_ch. */
    bracket_set = charalloc((mb_cur_max() * 2) + 1);
    strncpy(bracket_set, ch, ch_len);
    strncpy(bracket_set + ch_len, wanted_ch, wanted_ch_len);
    null_at(&bracket_set, ch_len + wanted_ch_len);

    found_ch = charalloc(mb_cur_max() + 1);

    while (TRUE) {
	if (find_bracket_match(reverse, bracket_set)) {
	    /* If we found an identical bracket, increment count.  If we
	     * found a complementary bracket, decrement it. */
	    parse_mbchar(openfile->current->data + openfile->current_x,
		found_ch, NULL);
	    count += (strncmp(found_ch, ch, ch_len) == 0) ? 1 : -1;

	    /* If count is zero, we've found a matching bracket.  Update
	     * the screen and get out. */
	    if (count == 0) {
		edit_redraw(current_save, pww_save);
		break;
	    }
	} else {
	    /* We didn't find either an opening or closing bracket.
	     * Indicate this, restore where we were, and get out. */
	    statusbar(_("No matching bracket"));
	    openfile->current = current_save;
	    openfile->current_x = current_x_save;
	    openfile->placewewant = pww_save;
	    break;
	}
    }

    /* Clean up. */
    free(bracket_set);
    free(found_ch);
}
#endif /* !NANO_TINY */

#ifndef DISABLE_HISTORIES
/* Indicate whether any of the history lists have changed. */
bool history_has_changed(void)
{
    return history_changed;
}

/* Initialize the search and replace history lists. */
void history_init(void)
{
    search_history = make_new_node(NULL);
    search_history->data = mallocstrcpy(NULL, "");
    searchage = search_history;
    searchbot = search_history;

    replace_history = make_new_node(NULL);
    replace_history->data = mallocstrcpy(NULL, "");
    replaceage = replace_history;
    replacebot = replace_history;
}

/* Set the current position in the history list h to the bottom. */
void history_reset(const filestruct *h)
{
    if (h == search_history)
	search_history = searchbot;
    else if (h == replace_history)
	replace_history = replacebot;
}

/* Return the first node containing the first len characters of the
 * string s in the history list, starting at h_start and ending at
 * h_end, or NULL if there isn't one. */
filestruct *find_history(const filestruct *h_start, const filestruct
	*h_end, const char *s, size_t len)
{
    const filestruct *p;

    for (p = h_start; p != h_end->next && p != NULL; p = p->next) {
	if (strncmp(s, p->data, len) == 0)
	    return (filestruct *)p;
    }

    return NULL;
}

/* Update a history list.  h should be the current position in the
 * list. */
void update_history(filestruct **h, const char *s)
{
    filestruct **hage = NULL, **hbot = NULL, *p;

    assert(h != NULL && s != NULL);

    if (*h == search_history) {
	hage = &searchage;
	hbot = &searchbot;
    } else if (*h == replace_history) {
	hage = &replaceage;
	hbot = &replacebot;
    }

    assert(hage != NULL && hbot != NULL);

    /* If this string is already in the history, delete it. */
    p = find_history(*hage, *hbot, s, strlen(s));

    if (p != NULL) {
	filestruct *foo, *bar;

	/* If the string is at the beginning, move the beginning down to
	 * the next string. */
	if (p == *hage)
	    *hage = (*hage)->next;

	/* Delete the string. */
	foo = p;
	bar = p->next;
	unlink_node(foo);
	renumber(bar);
    }

    /* If the history is full, delete the beginning entry to make room
     * for the new entry at the end.  We assume that MAX_SEARCH_HISTORY
     * is greater than zero. */
    if ((*hbot)->lineno == MAX_SEARCH_HISTORY + 1) {
	filestruct *foo = *hage;

	*hage = (*hage)->next;
	unlink_node(foo);
	renumber(*hage);
    }

    /* Add the new entry to the end. */
    (*hbot)->data = mallocstrcpy((*hbot)->data, s);
    splice_node(*hbot, make_new_node(*hbot));
    *hbot = (*hbot)->next;
    (*hbot)->data = mallocstrcpy(NULL, "");

    /* Indicate that the history's been changed. */
    history_changed = TRUE;

    /* Set the current position in the list to the bottom. */
    *h = *hbot;
}

/* Move h to the string in the history list just before it, and return
 * that string.  If there isn't one, don't move h and return NULL. */
char *get_history_older(filestruct **h)
{
    assert(h != NULL);

    if ((*h)->prev == NULL)
	return NULL;

    *h = (*h)->prev;

    return (*h)->data;
}

/* Move h to the string in the history list just after it, and return
 * that string.  If there isn't one, don't move h and return NULL. */
char *get_history_newer(filestruct **h)
{
    assert(h != NULL);

    if ((*h)->next == NULL)
	return NULL;

    *h = (*h)->next;

    return (*h)->data;
}

/* More placeholders. */
void get_history_newer_void(void)
{
    ;
}
void get_history_older_void(void)
{
    ;
}

#ifndef DISABLE_TABCOMP
/* Move h to the next string that's a tab completion of the string s,
 * looking at only the first len characters of s, and return that
 * string.  If there isn't one, or if len is 0, don't move h and return
 * s. */
char *get_history_completion(filestruct **h, char *s, size_t len)
{
    assert(s != NULL);

    if (len > 0) {
	filestruct *hage = NULL, *hbot = NULL, *p;

	assert(h != NULL);

	if (*h == search_history) {
	    hage = searchage;
	    hbot = searchbot;
	} else if (*h == replace_history) {
	    hage = replaceage;
	    hbot = replacebot;
	}

	assert(hage != NULL && hbot != NULL);

	/* Search the history list from the current position to the
	 * bottom for a match of len characters.  Skip over an exact
	 * match. */
	p = find_history((*h)->next, hbot, s, len);

	while (p != NULL && strcmp(p->data, s) == 0)
	    p = find_history(p->next, hbot, s, len);

	if (p != NULL) {
	    *h = p;
	    return mallocstrcpy(s, (*h)->data);
	}

	/* Search the history list from the top to the current position
	 * for a match of len characters.  Skip over an exact match. */
	p = find_history(hage, *h, s, len);

	while (p != NULL && strcmp(p->data, s) == 0)
	    p = find_history(p->next, *h, s, len);

	if (p != NULL) {
	    *h = p;
	    return mallocstrcpy(s, (*h)->data);
	}
    }

    /* If we're here, we didn't find a match, we didn't find an inexact
     * match, or len is 0.  Return s. */
    return (char *)s;
}
#endif /* !DISABLE_TABCOMP */
#endif /* !DISABLE_HISTORIES */
