/* $Id: nano.c 5678 2016-02-25 18:58:17Z bens $ */
/**************************************************************************
 *   nano.c                                                               *
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

#include <stdio.h>
#include <stdarg.h>
#include <unistd.h>
#include <string.h>
#include <fcntl.h>
#include <errno.h>
#include <ctype.h>
#include <locale.h>
#ifdef ENABLE_UTF8
#include <langinfo.h>
#endif
#include <termios.h>
#ifdef HAVE_GETOPT_H
#include <getopt.h>
#endif
#ifndef NANO_TINY
#include <sys/ioctl.h>
#endif

#ifndef DISABLE_MOUSE
static int oldinterval = -1;
	/* Used to store the user's original mouse click interval. */
#endif
#ifndef DISABLE_NANORC
static bool no_rcfiles = FALSE;
	/* Should we ignore all rcfiles? */
#endif
static struct termios oldterm;
	/* The user's original terminal settings. */
static struct sigaction act;
	/* Used to set up all our fun signal handlers. */

/* Create a new filestruct node.  Note that we do not set prevnode->next
 * to the new line. */
filestruct *make_new_node(filestruct *prevnode)
{
    filestruct *newnode = (filestruct *)nmalloc(sizeof(filestruct));

    newnode->data = NULL;
    newnode->prev = prevnode;
    newnode->next = NULL;
    newnode->lineno = (prevnode != NULL) ? prevnode->lineno + 1 : 1;

#ifndef DISABLE_COLOR
    newnode->multidata = NULL;
#endif

    return newnode;
}

/* Make a copy of a filestruct node. */
filestruct *copy_node(const filestruct *src)
{
    filestruct *dst;

    assert(src != NULL);

    dst = (filestruct *)nmalloc(sizeof(filestruct));

    dst->data = mallocstrcpy(NULL, src->data);
    dst->next = src->next;
    dst->prev = src->prev;
    dst->lineno = src->lineno;
#ifndef DISABLE_COLOR
    dst->multidata = NULL;
#endif

    return dst;
}

/* Splice a new node into an existing linked list of filestructs. */
void splice_node(filestruct *afterthis, filestruct *newnode)
{
    assert(afterthis != NULL && newnode != NULL);

    newnode->next = afterthis->next;
    newnode->prev = afterthis;
    if (afterthis->next != NULL)
	afterthis->next->prev = newnode;
    afterthis->next = newnode;

    /* Update filebot when inserting a node at the end of file. */
    if (openfile && openfile->filebot == afterthis)
	openfile->filebot = newnode;
}

/* Disconnect a node from a linked list of filestructs and delete it. */
void unlink_node(filestruct *fileptr)
{
    assert(fileptr != NULL);

    if (fileptr->prev != NULL)
	fileptr->prev->next = fileptr->next;
    if (fileptr->next != NULL)
	fileptr->next->prev = fileptr->prev;

    /* Update filebot when removing a node at the end of file. */
    if (openfile && openfile->filebot == fileptr)
	openfile->filebot = fileptr->prev;

    delete_node(fileptr);
}

/* Free the data structures in the given node. */
void delete_node(filestruct *fileptr)
{
    assert(fileptr != NULL && fileptr->data != NULL);

    free(fileptr->data);
#ifndef DISABLE_COLOR
    free(fileptr->multidata);
#endif
    free(fileptr);
}

/* Duplicate a whole filestruct. */
filestruct *copy_filestruct(const filestruct *src)
{
    filestruct *head, *copy;

    assert(src != NULL);

    copy = copy_node(src);
    copy->prev = NULL;
    head = copy;
    src = src->next;

    while (src != NULL) {
	copy->next = copy_node(src);
	copy->next->prev = copy;
	copy = copy->next;

	src = src->next;
    }

    copy->next = NULL;

    return head;
}

/* Free a whole linked list of filestructs. */
void free_filestruct(filestruct *src)
{
    if (src == NULL)
	return;

    while (src->next != NULL) {
	src = src->next;
	delete_node(src->prev);
    }

    delete_node(src);
}

/* Renumber all entries in a filestruct, starting with fileptr. */
void renumber(filestruct *fileptr)
{
    ssize_t line;

    if (fileptr == NULL)
	return;

    line = (fileptr->prev == NULL) ? 0 : fileptr->prev->lineno;

    assert(fileptr != fileptr->next);

    for (; fileptr != NULL; fileptr = fileptr->next)
	fileptr->lineno = ++line;
}

/* Partition a filestruct so that it begins at (top, top_x) and ends at
 * (bot, bot_x). */
partition *partition_filestruct(filestruct *top, size_t top_x,
	filestruct *bot, size_t bot_x)
{
    partition *p;

    assert(top != NULL && bot != NULL && openfile->fileage != NULL && openfile->filebot != NULL);

    /* Initialize the partition. */
    p = (partition *)nmalloc(sizeof(partition));

    /* If the top and bottom of the partition are different from the top
     * and bottom of the filestruct, save the latter and then set them
     * to top and bot. */
    if (top != openfile->fileage) {
	p->fileage = openfile->fileage;
	openfile->fileage = top;
    } else
	p->fileage = NULL;
    if (bot != openfile->filebot) {
	p->filebot = openfile->filebot;
	openfile->filebot = bot;
    } else
	p->filebot = NULL;

    /* Save the line above the top of the partition, detach the top of
     * the partition from it, and save the text before top_x in
     * top_data. */
    p->top_prev = top->prev;
    top->prev = NULL;
    p->top_data = mallocstrncpy(NULL, top->data, top_x + 1);
    p->top_data[top_x] = '\0';

    /* Save the line below the bottom of the partition, detach the
     * bottom of the partition from it, and save the text after bot_x in
     * bot_data. */
    p->bot_next = bot->next;
    bot->next = NULL;
    p->bot_data = mallocstrcpy(NULL, bot->data + bot_x);

    /* Remove all text after bot_x at the bottom of the partition. */
    null_at(&bot->data, bot_x);

    /* Remove all text before top_x at the top of the partition. */
    charmove(top->data, top->data + top_x, strlen(top->data) -
	top_x + 1);
    align(&top->data);

    /* Return the partition. */
    return p;
}

/* Unpartition a filestruct so that it begins at (fileage, 0) and ends
 * at (filebot, strlen(filebot->data)) again. */
void unpartition_filestruct(partition **p)
{
    char *tmp;

    assert(p != NULL && openfile->fileage != NULL && openfile->filebot != NULL);

    /* Reattach the line above the top of the partition, and restore the
     * text before top_x from top_data.  Free top_data when we're done
     * with it. */
    tmp = mallocstrcpy(NULL, openfile->fileage->data);
    openfile->fileage->prev = (*p)->top_prev;
    if (openfile->fileage->prev != NULL)
	openfile->fileage->prev->next = openfile->fileage;
    openfile->fileage->data = charealloc(openfile->fileage->data,
	strlen((*p)->top_data) + strlen(openfile->fileage->data) + 1);
    strcpy(openfile->fileage->data, (*p)->top_data);
    free((*p)->top_data);
    strcat(openfile->fileage->data, tmp);
    free(tmp);

    /* Reattach the line below the bottom of the partition, and restore
     * the text after bot_x from bot_data.  Free bot_data when we're
     * done with it. */
    openfile->filebot->next = (*p)->bot_next;
    if (openfile->filebot->next != NULL)
	openfile->filebot->next->prev = openfile->filebot;
    openfile->filebot->data = charealloc(openfile->filebot->data,
	strlen(openfile->filebot->data) + strlen((*p)->bot_data) + 1);
    strcat(openfile->filebot->data, (*p)->bot_data);
    free((*p)->bot_data);

    /* Restore the top and bottom of the filestruct, if they were
     * different from the top and bottom of the partition. */
    if ((*p)->fileage != NULL)
	openfile->fileage = (*p)->fileage;
    if ((*p)->filebot != NULL)
	openfile->filebot = (*p)->filebot;

    /* Uninitialize the partition. */
    free(*p);
    *p = NULL;
}

/* Move all the text between (top, top_x) and (bot, bot_x) in the
 * current filestruct to a filestruct beginning with file_top and ending
 * with file_bot.  If no text is between (top, top_x) and (bot, bot_x),
 * don't do anything. */
void move_to_filestruct(filestruct **file_top, filestruct **file_bot,
	filestruct *top, size_t top_x, filestruct *bot, size_t bot_x)
{
    filestruct *top_save;
    bool edittop_inside;
#ifndef NANO_TINY
    bool mark_inside = FALSE;
    bool same_line = FALSE;
#endif

    assert(file_top != NULL && file_bot != NULL && top != NULL && bot != NULL);

    /* If (top, top_x)-(bot, bot_x) doesn't cover any text, get out. */
    if (top == bot && top_x == bot_x)
	return;

    /* Partition the filestruct so that it contains only the text from
     * (top, top_x) to (bot, bot_x), keep track of whether the top of
     * the edit window is inside the partition, and keep track of
     * whether the mark begins inside the partition. */
    filepart = partition_filestruct(top, top_x, bot, bot_x);
    edittop_inside = (openfile->edittop->lineno >=
	openfile->fileage->lineno && openfile->edittop->lineno <=
	openfile->filebot->lineno);
#ifndef NANO_TINY
    if (openfile->mark_set) {
	mark_inside = (openfile->mark_begin->lineno >=
		openfile->fileage->lineno &&
		openfile->mark_begin->lineno <=
		openfile->filebot->lineno &&
		(openfile->mark_begin != openfile->fileage ||
		openfile->mark_begin_x >= top_x) &&
		(openfile->mark_begin != openfile->filebot ||
		openfile->mark_begin_x <= bot_x));
	same_line = (openfile->mark_begin == openfile->fileage);
    }
#endif

    /* Get the number of characters in the text, and subtract it from
     * totsize. */
    openfile->totsize -= get_totsize(top, bot);

    if (*file_top == NULL) {
	/* If file_top is empty, just move all the text directly into
	 * it.  This is equivalent to tacking the text in top onto the
	 * (lack of) text at the end of file_top. */
	*file_top = openfile->fileage;
	*file_bot = openfile->filebot;

	/* Renumber starting with file_top. */
	renumber(*file_top);
    } else {
	filestruct *file_bot_save = *file_bot;

	/* Otherwise, tack the text in top onto the text at the end of
	 * file_bot. */
	(*file_bot)->data = charealloc((*file_bot)->data,
		strlen((*file_bot)->data) +
		strlen(openfile->fileage->data) + 1);
	strcat((*file_bot)->data, openfile->fileage->data);

	/* Attach the line after top to the line after file_bot.  Then,
	 * if there's more than one line after top, move file_bot down
	 * to bot. */
	(*file_bot)->next = openfile->fileage->next;
	if ((*file_bot)->next != NULL) {
	    (*file_bot)->next->prev = *file_bot;
	    *file_bot = openfile->filebot;
	}

	delete_node(openfile->fileage);

	/* Renumber starting with the line after the original
	 * file_bot. */
	renumber(file_bot_save->next);
    }

    /* Since the text has now been saved, remove it from the
     * filestruct. */
    openfile->fileage = (filestruct *)nmalloc(sizeof(filestruct));
    openfile->fileage->data = mallocstrcpy(NULL, "");
    openfile->filebot = openfile->fileage;

#ifndef DISABLE_COLOR
    openfile->fileage->multidata = NULL;
#endif

    /* Restore the current line and cursor position.  If the mark begins
     * inside the partition, set the beginning of the mark to where the
     * saved text used to start. */
    openfile->current = openfile->fileage;
    openfile->current_x = top_x;
#ifndef NANO_TINY
    if (mark_inside) {
	openfile->mark_begin = openfile->current;
	openfile->mark_begin_x = openfile->current_x;
    } else if (same_line)
	/* Update the pointer to this partially cut line. */
	openfile->mark_begin = openfile->current;
#endif

    top_save = openfile->fileage;

    /* Unpartition the filestruct so that it contains all the text
     * again, minus the saved text. */
    unpartition_filestruct(&filepart);

    /* If the top of the edit window was inside the old partition, put
     * it in range of current. */
    if (edittop_inside)
	edit_update(NONE);

    /* Renumber starting with the beginning line of the old
     * partition. */
    renumber(top_save);

    /* If the NO_NEWLINES flag isn't set, and the text doesn't end with
     * a magicline, add a new magicline. */
    if (!ISSET(NO_NEWLINES) && openfile->filebot->data[0] != '\0')
	new_magicline();
}

/* Copy all text from the given filestruct to the current filestruct
 * at the current cursor position. */
void copy_from_filestruct(filestruct *somebuffer)
{
    filestruct *top_save;
    size_t current_x_save = openfile->current_x;
    bool edittop_inside;
#ifndef NANO_TINY
    bool right_side_up = FALSE, single_line = FALSE;
#endif

    assert(somebuffer != NULL);

#ifndef NANO_TINY
    /* Keep track of whether the mark begins inside the partition and
     * will need adjustment. */
    if (openfile->mark_set) {
	filestruct *top, *bot;
	size_t top_x, bot_x;

	mark_order((const filestruct **)&top, &top_x,
		(const filestruct **)&bot, &bot_x, &right_side_up);

	single_line = (top == bot);
    }
#endif

    /* Partition the filestruct so that it contains no text, and keep
     * track of whether the top of the edit window is inside the
     * partition. */
    filepart = partition_filestruct(openfile->current,
	openfile->current_x, openfile->current, openfile->current_x);
    edittop_inside = (openfile->edittop == openfile->fileage);

    /* Put the top and bottom of the current filestruct at the top and
     * bottom of a copy of the passed buffer. */
    free_filestruct(openfile->fileage);
    openfile->fileage = copy_filestruct(somebuffer);
    openfile->filebot = openfile->fileage;
    while (openfile->filebot->next != NULL)
	openfile->filebot = openfile->filebot->next;

    /* Put the cursor at the end of the pasted text. */
    openfile->current = openfile->filebot;
    openfile->current_x = strlen(openfile->filebot->data);

    /* Refresh the mark's pointer, and compensate the mark's
     * x coordinate for the change in the current line. */
    if (openfile->fileage == openfile->filebot) {
#ifndef NANO_TINY
	if (openfile->mark_set && single_line) {
	    openfile->mark_begin = openfile->current;
	    if (!right_side_up)
		openfile->mark_begin_x += openfile->current_x;
	}
#endif
	/* When the pasted stuff contains no newline, adjust the cursor's
	 * x coordinate for the text that is before the pasted stuff. */
	openfile->current_x += current_x_save;
    }
#ifndef NANO_TINY
    else if (openfile->mark_set && single_line) {
	if (right_side_up)
	    openfile->mark_begin = openfile->fileage;
	else {
	    openfile->mark_begin = openfile->current;
	    openfile->mark_begin_x += openfile->current_x - current_x_save;
	}
    }
#endif

    /* Get the number of characters in the copied text, and add it to
     * totsize. */
    openfile->totsize += get_totsize(openfile->fileage,
	openfile->filebot);

    /* Update the current y-coordinate to account for the number of
     * lines the copied text has, less one since the first line will be
     * tacked onto the current line. */
    openfile->current_y += openfile->filebot->lineno - 1;

    top_save = openfile->fileage;

    /* If the top of the edit window is inside the partition, set it to
     * where the copied text now starts. */
    if (edittop_inside)
	openfile->edittop = openfile->fileage;

    /* Unpartition the filestruct so that it contains all the text
     * again, plus the copied text. */
    unpartition_filestruct(&filepart);

    /* Renumber starting with the beginning line of the old
     * partition. */
    renumber(top_save);

    /* If the NO_NEWLINES flag isn't set, and the text doesn't end with
     * a magicline, add a new magicline. */
    if (!ISSET(NO_NEWLINES) && openfile->filebot->data[0] != '\0')
	new_magicline();
}

/* Create a new openfilestruct node. */
openfilestruct *make_new_opennode(void)
{
    return (openfilestruct *)nmalloc(sizeof(openfilestruct));
}

/* Unlink a node from the rest of the openfilestruct, and delete it. */
void unlink_opennode(openfilestruct *fileptr)
{
    assert(fileptr != NULL && fileptr->prev != NULL && fileptr->next != NULL &&
		 fileptr != fileptr->prev && fileptr != fileptr->next);

    fileptr->prev->next = fileptr->next;
    fileptr->next->prev = fileptr->prev;

    delete_opennode(fileptr);
}

/* Free all the memory in the given open-file node. */
void delete_opennode(openfilestruct *fileptr)
{
    assert(fileptr != NULL && fileptr->filename != NULL && fileptr->fileage != NULL);

    free(fileptr->filename);
    free_filestruct(fileptr->fileage);
#ifndef NANO_TINY
    free(fileptr->current_stat);
    free(fileptr->lock_filename);
    /* Free the undo stack. */
    discard_until(NULL, fileptr);
#endif
    free(fileptr);
}

/* Display a warning about a key disabled in view mode. */
void print_view_warning(void)
{
    statusbar(_("Key is invalid in view mode"));
}

/* Indicate that something is disabled in restricted mode. */
void show_restricted_warning(void)
{
    statusbar(_("This function is disabled in restricted mode"));
    beep();
}

#ifdef DISABLE_HELP
/* Indicate that help texts are unavailable. */
void say_there_is_no_help(void)
{
    statusbar(_("Help is not available"));
}
#endif

/* Make nano exit gracefully. */
void finish(void)
{
    /* Blank the statusbar and (if applicable) the shortcut list,
     * and move the cursor to the last line of the screen. */
    blank_statusbar();
    blank_bottombars();
    wrefresh(bottomwin);
    endwin();

    /* Restore the old terminal settings. */
    tcsetattr(0, TCSANOW, &oldterm);

#ifndef DISABLE_HISTORIES
    if (ISSET(HISTORYLOG))
	save_history();
    if (ISSET(POS_HISTORY)) {
	update_poshistory(openfile->filename, openfile->current->lineno, xplustabs() + 1);
	save_poshistory();
    }
#endif

#ifdef DEBUG
    thanks_for_all_the_fish();
#endif

    /* Get out. */
    exit(0);
}

/* Make nano die gracefully. */
void die(const char *msg, ...)
{
    va_list ap;

    endwin();

    /* Restore the old terminal settings. */
    tcsetattr(0, TCSANOW, &oldterm);

    va_start(ap, msg);
    vfprintf(stderr, msg, ap);
    va_end(ap);

    /* Save the current file buffer if it's been modified. */
    if (openfile && openfile->modified) {
	/* If we've partitioned the filestruct, unpartition it now. */
	if (filepart != NULL)
	    unpartition_filestruct(&filepart);

	die_save_file(openfile->filename
#ifndef NANO_TINY
		, openfile->current_stat
#endif
		);
    }

#ifndef DISABLE_MULTIBUFFER
    /* Save all of the other modified file buffers, if any. */
    if (openfile != NULL) {
	openfilestruct *tmp = openfile;

	while (tmp != openfile->next) {
	    openfile = openfile->next;

	    /* Save the current file buffer if it's been modified. */
	    if (openfile->modified)
		die_save_file(openfile->filename
#ifndef NANO_TINY
			, openfile->current_stat
#endif
			);
	}
    }
#endif

    /* Get out. */
    exit(1);
}

/* Save the current file under the name spacified in die_filename, which
 * is modified to be unique if necessary. */
void die_save_file(const char *die_filename
#ifndef NANO_TINY
	, struct stat *die_stat
#endif
	)
{
    char *retval;
    bool failed = TRUE;

    /* If we're using restricted mode, don't write any emergency backup
     * files, since that would allow reading from or writing to files
     * not specified on the command line. */
    if (ISSET(RESTRICTED))
	return;

    /* If we can't save, we have really bad problems, but we might as
     * well try. */
    if (*die_filename == '\0')
	die_filename = "nano";

    retval = get_next_filename(die_filename, ".save");
    if (retval[0] != '\0')
	failed = !write_file(retval, NULL, TRUE, OVERWRITE, TRUE);

    if (!failed)
	fprintf(stderr, _("\nBuffer written to %s\n"), retval);
    else if (retval[0] != '\0')
	fprintf(stderr, _("\nBuffer not written to %s: %s\n"), retval,
		strerror(errno));
    else
	fprintf(stderr, _("\nBuffer not written: %s\n"),
		_("Too many backup files?"));

#ifndef NANO_TINY
    /* Try and chmod/chown the save file to the values of the original file,
     * but don't worry if it fails because we're supposed to be bailing as
     * fast as possible. */
    if (die_stat) {
	int shush;
	shush = chmod(retval, die_stat->st_mode);
	shush = chown(retval, die_stat->st_uid, die_stat->st_gid);
	if (shush)
	    ;
    }
#endif

    free(retval);
}

/* Initialize the three window portions nano uses. */
void window_init(void)
{
    /* If the screen height is too small, get out. */
    editwinrows = LINES - 5 + more_space() + no_help();
    if (COLS < MIN_EDITOR_COLS || editwinrows < MIN_EDITOR_ROWS)
	die(_("Window size is too small for nano...\n"));

#ifndef DISABLE_WRAPJUSTIFY
    /* Set up fill, based on the screen width. */
    fill = wrap_at;
    if (fill <= 0)
	fill += COLS;
    if (fill < 0)
	fill = 0;
#endif

    if (topwin != NULL)
	delwin(topwin);
    if (edit != NULL)
	delwin(edit);
    if (bottomwin != NULL)
	delwin(bottomwin);

    /* Set up the windows. */
    topwin = newwin(2 - more_space(), COLS, 0, 0);
    edit = newwin(editwinrows, COLS, 2 - more_space(), 0);
    bottomwin = newwin(3 - no_help(), COLS, editwinrows + (2 -
	more_space()), 0);

    /* Turn the keypad on for the windows, if necessary. */
    if (!ISSET(REBIND_KEYPAD)) {
	keypad(topwin, TRUE);
	keypad(edit, TRUE);
	keypad(bottomwin, TRUE);
    }
}

#ifndef DISABLE_MOUSE
/* Disable mouse support. */
void disable_mouse_support(void)
{
    mousemask(0, NULL);
    mouseinterval(oldinterval);
}

/* Enable mouse support. */
void enable_mouse_support(void)
{
    mousemask(ALL_MOUSE_EVENTS, NULL);
    oldinterval = mouseinterval(50);
}

/* Initialize mouse support.  Enable it if the USE_MOUSE flag is set,
 * and disable it otherwise. */
void mouse_init(void)
{
    if (ISSET(USE_MOUSE))
	enable_mouse_support();
    else
	disable_mouse_support();
}
#endif /* !DISABLE_MOUSE */

#ifdef HAVE_GETOPT_LONG
#define print_opt(shortflag, longflag, desc) print_opt_full(shortflag, longflag, desc)
#else
#define print_opt(shortflag, longflag, desc) print_opt_full(shortflag, desc)
#endif

/* Print one usage string to the screen.  This cuts down on duplicate
 * strings to translate, and leaves out the parts that shouldn't be
 * translatable (i.e. the flag names). */
void print_opt_full(const char *shortflag
#ifdef HAVE_GETOPT_LONG
	, const char *longflag
#endif
	, const char *desc)
{
    printf(" %s\t", shortflag);
    if (strlenpt(shortflag) < 8)
	printf("\t");

#ifdef HAVE_GETOPT_LONG
    printf("%s\t", longflag);
    if (strlenpt(longflag) < 8)
	printf("\t\t");
    else if (strlenpt(longflag) < 16)
	printf("\t");
#endif

    if (desc != NULL)
	printf("%s", _(desc));
    printf("\n");
}

/* Explain how to properly use nano and its command-line options. */
void usage(void)
{
    printf(_("Usage: nano [OPTIONS] [[+LINE,COLUMN] FILE]...\n\n"));
    printf(
#ifdef HAVE_GETOPT_LONG
	_("Option\t\tGNU long option\t\tMeaning\n")
#else
	_("Option\t\tMeaning\n")
#endif
	);
    print_opt(_("+LINE,COLUMN"), "",
	/* TRANSLATORS: The next forty or so strings are option descriptions
	 * for the --help output.  Try to keep them at most 40 characters. */
	N_("Start at line LINE, column COLUMN"));
#ifndef NANO_TINY
    print_opt("-A", "--smarthome", N_("Enable smart home key"));
    if (!ISSET(RESTRICTED)) {
	print_opt("-B", "--backup", N_("Save backups of existing files"));
	print_opt(_("-C <dir>"), _("--backupdir=<dir>"),
		N_("Directory for saving unique backup files"));
    }
#endif
    print_opt("-D", "--boldtext", N_("Use bold instead of reverse video text"));
#ifndef NANO_TINY
    print_opt("-E", "--tabstospaces", N_("Convert typed tabs to spaces"));
#endif
#ifndef DISABLE_MULTIBUFFER
    if (!ISSET(RESTRICTED))
	print_opt("-F", "--multibuffer",
		N_("Read a file into a new buffer by default"));
#endif
#ifndef NANO_TINY
    print_opt("-G", "--locking", N_("Use (vim-style) lock files"));
#endif
#ifndef DISABLE_HISTORIES
    if (!ISSET(RESTRICTED))
	print_opt("-H", "--historylog",
		N_("Log & read search/replace string history"));
#endif
#ifndef DISABLE_NANORC
    if (!ISSET(RESTRICTED))
	print_opt("-I", "--ignorercfiles", N_("Don't look at nanorc files"));
#endif
    print_opt("-K", "--rebindkeypad",
	N_("Fix numeric keypad key confusion problem"));
    print_opt("-L", "--nonewlines",
	N_("Don't add newlines to the ends of files"));
#ifndef NANO_TINY
    print_opt("-N", "--noconvert",
	N_("Don't convert files from DOS/Mac format"));
#endif
    print_opt("-O", "--morespace", N_("Use one more line for editing"));
#ifndef DISABLE_HISTORIES
    if (!ISSET(RESTRICTED))
	print_opt("-P", "--positionlog",
		N_("Log & read location of cursor position"));
#endif
#ifndef DISABLE_JUSTIFY
    print_opt(_("-Q <str>"), _("--quotestr=<str>"), N_("Quoting string"));
#endif
    if (!ISSET(RESTRICTED))
	print_opt("-R", "--restricted", N_("Restricted mode"));
#ifndef NANO_TINY
    print_opt("-S", "--smooth", N_("Scroll by line instead of half-screen"));
#endif
    print_opt(_("-T <#cols>"), _("--tabsize=<#cols>"),
	N_("Set width of a tab to #cols columns"));
#ifndef NANO_TINY
    print_opt("-U", "--quickblank", N_("Do quick statusbar blanking"));
#endif
    print_opt("-V", "--version", N_("Print version information and exit"));
#ifndef NANO_TINY
    print_opt("-W", "--wordbounds",
	N_("Detect word boundaries more accurately"));
#endif
#ifndef DISABLE_COLOR
    if (!ISSET(RESTRICTED))
	print_opt(_("-Y <str>"), _("--syntax=<str>"),
		N_("Syntax definition to use for coloring"));
#endif
    print_opt("-c", "--constantshow", N_("Constantly show cursor position"));
    print_opt("-d", "--rebinddelete",
	N_("Fix Backspace/Delete confusion problem"));
    print_opt("-h", "--help", N_("Show this help text and exit"));
#ifndef NANO_TINY
    print_opt("-i", "--autoindent", N_("Automatically indent new lines"));
    print_opt("-k", "--cut", N_("Cut from cursor to end of line"));
#endif
#ifndef DISABLE_MOUSE
    print_opt("-m", "--mouse", N_("Enable the use of the mouse"));
#endif
    print_opt("-n", "--noread", N_("Do not read the file (only write it)"));
#ifndef DISABLE_OPERATINGDIR
    print_opt(_("-o <dir>"), _("--operatingdir=<dir>"),
	N_("Set operating directory"));
#endif
    print_opt("-p", "--preserve", N_("Preserve XON (^Q) and XOFF (^S) keys"));
#ifndef DISABLE_NANORC
    if (!ISSET(RESTRICTED))
	print_opt("-q", "--quiet",
		N_("Silently ignore startup issues like rc file errors"));
#endif
#ifndef DISABLE_WRAPJUSTIFY
    print_opt(_("-r <#cols>"), _("--fill=<#cols>"),
	N_("Set hard-wrapping point at column #cols"));
#endif
#ifndef DISABLE_SPELLER
    if (!ISSET(RESTRICTED))
	print_opt(_("-s <prog>"), _("--speller=<prog>"),
		N_("Enable alternate speller"));
#endif
    print_opt("-t", "--tempfile", N_("Auto save on exit, don't prompt"));
#ifndef NANO_TINY
    print_opt("-u", "--unix", N_("Save a file by default in Unix format"));
#endif
    print_opt("-v", "--view", N_("View mode (read-only)"));
#ifndef DISABLE_WRAPPING
    print_opt("-w", "--nowrap", N_("Don't hard-wrap long lines"));
#endif
    print_opt("-x", "--nohelp", N_("Don't show the two help lines"));
    if (!ISSET(RESTRICTED))
	print_opt("-z", "--suspend", N_("Enable suspension"));
#ifndef NANO_TINY
    print_opt("-$", "--softwrap", N_("Enable soft line wrapping"));
#endif
}

/* Display the current version of nano, the date and time it was
 * compiled, contact information for it, and the configuration options
 * it was compiled with. */
void version(void)
{
    printf(_(" GNU nano, version %s\n"), VERSION);
    printf(" (C) 1999..2016 Free Software Foundation, Inc.\n");
    printf(
	_(" Email: nano@nano-editor.org	Web: http://www.nano-editor.org/"));
    printf(_("\n Compiled options:"));

#ifdef NANO_TINY
    printf(" --enable-tiny");
#ifndef DISABLE_BROWSER
    printf(" --enable-browser");
#endif
#ifndef DISABLE_COLOR
    printf(" --enable-color");
#endif
#ifndef DISABLE_EXTRA
    printf(" --enable-extra");
#endif
#ifndef DISABLE_HELP
    printf(" --enable-help");
#endif
#ifndef DISABLE_HISTORIES
    printf(" --enable-histories");
#endif
#ifndef DISABLE_JUSTIFY
    printf(" --enable-justify");
#endif
#ifdef HAVE_LIBMAGIC
    printf(" --enable-libmagic");
#endif
#ifndef DISABLE_MOUSE
    printf(" --enable-mouse");
#endif
#ifndef DISABLE_NANORC
    printf(" --enable-nanorc");
#endif
#ifndef DISABLE_MULTIBUFFER
    printf(" --enable-multibuffer");
#endif
#ifndef DISABLE_OPERATINGDIR
    printf(" --enable-operatingdir");
#endif
#ifndef DISABLE_SPELLER
    printf(" --enable-speller");
#endif
#ifndef DISABLE_TABCOMP
    printf(" --enable-tabcomp");
#endif
#ifndef DISABLE_WRAPPING
    printf(" --enable-wrapping");
#endif
#else /* !NANO_TINY */
#ifdef DISABLE_BROWSER
    printf(" --disable-browser");
#endif
#ifdef DISABLE_COLOR
    printf(" --disable-color");
#endif
#ifdef DISABLE_EXTRA
    printf(" --disable-extra");
#endif
#ifdef DISABLE_HELP
    printf(" --disable-help");
#endif
#ifdef DISABLE_HISTORIES
    printf(" --disable-histories");
#endif
#ifdef DISABLE_JUSTIFY
    printf(" --disable-justify");
#endif
#ifndef HAVE_LIBMAGIC
    printf(" --disable-libmagic");
#endif
#ifdef DISABLE_MOUSE
    printf(" --disable-mouse");
#endif
#ifdef DISABLE_MULTIBUFFER
    printf(" --disable-multibuffer");
#endif
#ifdef DISABLE_NANORC
    printf(" --disable-nanorc");
#endif
#ifdef DISABLE_OPERATINGDIR
    printf(" --disable-operatingdir");
#endif
#ifdef DISABLE_SPELLER
    printf(" --disable-speller");
#endif
#ifdef DISABLE_TABCOMP
    printf(" --disable-tabcomp");
#endif
#ifdef DISABLE_WRAPPING
    printf(" --disable-wrapping");
#endif
#endif /* !NANO_TINY */

#ifdef DISABLE_ROOTWRAPPING
    printf(" --disable-wrapping-as-root");
#endif
#ifdef DEBUG
    printf(" --enable-debug");
#endif
#ifndef ENABLE_NLS
    printf(" --disable-nls");
#endif
#ifdef ENABLE_UTF8
    printf(" --enable-utf8");
#else
    printf(" --disable-utf8");
#endif
#ifdef USE_SLANG
    printf(" --with-slang");
#endif
    printf("\n");
}

/* Return 1 if the MORE_SPACE flag is set, and 0 otherwise.  This is
 * used to calculate the sizes and Y coordinates of the subwindows. */
int more_space(void)
{
    return ISSET(MORE_SPACE) ? 1 : 0;
}

/* Return 2 if the NO_HELP flag is set, and 0 otherwise.  This is used
 * to calculate the sizes and Y coordinates of the subwindows, because
 * having NO_HELP adds two lines to the edit window. */
int no_help(void)
{
    return ISSET(NO_HELP) ? 2 : 0;
}

/* Indicate that the current file has no name, in a way that gets the
 * user's attention.  This is used when trying to save a file with no
 * name with the TEMP_FILE flag set, just before the filename prompt. */
void no_current_file_name_warning(void)
{
    /* Warn that the current file has no name. */
    statusbar(_("No file name"));
    beep();

    /* Ensure that we see the warning. */
    napms(1800);

    curs_set(1);
}

/* If the current file buffer has been modified, and the TEMP_FILE flag
 * isn't set, ask whether or not to save the file buffer.  If the
 * TEMP_FILE flag is set and the current file has a name, save it
 * unconditionally.  Then, if more than one file buffer is open, close
 * the current file buffer and switch to the next one.  If only one file
 * buffer is open, exit from nano. */
void do_exit(void)
{
    int i;

    /* If the file hasn't been modified, pretend the user chose not to
     * save. */
    if (!openfile->modified)
	i = 0;
    /* If the TEMP_FILE flag is set and the current file has a name,
     * pretend the user chose to save. */
    else if (openfile->filename[0] != '\0' && ISSET(TEMP_FILE))
	i = 1;
    /* Otherwise, ask the user whether or not to save. */
    else {
	/* If the TEMP_FILE flag is set, and the current file doesn't
	 * have a name, warn the user before prompting for a name. */
	if (ISSET(TEMP_FILE))
	    no_current_file_name_warning();

	i = do_yesno_prompt(FALSE,
		_("Save modified buffer (ANSWERING \"No\" WILL DESTROY CHANGES) ? "));
    }

#ifdef DEBUG
    dump_filestruct(openfile->fileage);
#endif

    /* If the user chose not to save, or if the user chose to save and
     * the save succeeded, we're ready to exit. */
    if (i == 0 || (i == 1 && do_writeout(TRUE)))
	close_and_go();
    else if (i != 1)
	statusbar(_("Cancelled"));

    display_main_list();
}

/* Close the current buffer, and terminate nano if it was the last. */
void close_and_go(void)
{
#ifndef NANO_TINY
    /* If there is a lockfile, remove it. */
    if (ISSET(LOCKING) && openfile->lock_filename)
	delete_lockfile(openfile->lock_filename);
#endif
#ifndef DISABLE_MULTIBUFFER
    /* If there are no more open file buffers, jump off a cliff. */
    if (!close_buffer(FALSE))
#endif
	finish();
}

/* Another placeholder for function mapping. */
void do_cancel(void)
{
    ;
}

static struct sigaction pager_oldaction, pager_newaction;
	/* Original and temporary handlers for SIGINT. */
static bool pager_sig_failed = FALSE;
	/* Did sigaction() fail without changing the signal handlers? */
static bool pager_input_aborted = FALSE;
	/* Did someone invoke the pager and abort it via ^C? */

/* Things which need to be run regardless of whether
 * we finished the stdin pipe correctly or not. */
void finish_stdin_pager(void)
{
    FILE *f;
    int ttystdin;

    /* Read whatever we did get from stdin. */
    f = fopen("/dev/stdin", "rb");
    if (f == NULL)
	nperror("fopen");

    read_file(f, 0, "stdin", TRUE, FALSE);
    ttystdin = open("/dev/tty", O_RDONLY);
    if (!ttystdin)
	die(_("Couldn't reopen stdin from keyboard, sorry\n"));

    dup2(ttystdin,0);
    close(ttystdin);
    if (!pager_input_aborted)
	tcgetattr(0, &oldterm);
    if (!pager_sig_failed && sigaction(SIGINT, &pager_oldaction, NULL) == -1)
	nperror("sigaction");
    terminal_init();
    doupdate();
}

/* Cancel reading from stdin like a pager. */
RETSIGTYPE cancel_stdin_pager(int signal)
{
    pager_input_aborted = TRUE;
}

/* Let nano read stdin for the first file at least. */
void stdin_pager(void)
{
    endwin();
    if (!pager_input_aborted)
	tcsetattr(0, TCSANOW, &oldterm);
    fprintf(stderr, _("Reading from stdin, ^C to abort\n"));

    /* Enable interpretation of the special control keys so that
     * we get SIGINT when Ctrl-C is pressed. */
#ifndef NANO_TINY
    enable_signals();
#endif

    /* Set things up so that SIGINT will cancel the new process. */
    if (sigaction(SIGINT, NULL, &pager_newaction) == -1) {
	pager_sig_failed = TRUE;
	nperror("sigaction");
    } else {
	pager_newaction.sa_handler = cancel_stdin_pager;
	if (sigaction(SIGINT, &pager_newaction, &pager_oldaction) == -1) {
	    pager_sig_failed = TRUE;
	    nperror("sigaction");
	}
    }

    open_buffer("", FALSE);
    finish_stdin_pager();
}

/* Initialize the signal handlers. */
void signal_init(void)
{
    /* Trap SIGINT and SIGQUIT because we want them to do useful things. */
    memset(&act, 0, sizeof(struct sigaction));
    act.sa_handler = SIG_IGN;
    sigaction(SIGINT, &act, NULL);
    sigaction(SIGQUIT, &act, NULL);

    /* Trap SIGHUP and SIGTERM because we want to write the file out. */
    act.sa_handler = handle_hupterm;
    sigaction(SIGHUP, &act, NULL);
    sigaction(SIGTERM, &act, NULL);

#ifndef NANO_TINY
    /* Trap SIGWINCH because we want to handle window resizes. */
    act.sa_handler = handle_sigwinch;
    sigaction(SIGWINCH, &act, NULL);
#endif

    /* Trap normal suspend (^Z) so we can handle it ourselves. */
    if (!ISSET(SUSPEND)) {
	act.sa_handler = SIG_IGN;
	sigaction(SIGTSTP, &act, NULL);
    } else {
	/* Block all other signals in the suspend and continue handlers.
	 * If we don't do this, other stuff interrupts them! */
	sigfillset(&act.sa_mask);

	act.sa_handler = do_suspend;
	sigaction(SIGTSTP, &act, NULL);

	act.sa_handler = do_continue;
	sigaction(SIGCONT, &act, NULL);
    }
}

/* Handler for SIGHUP (hangup) and SIGTERM (terminate). */
RETSIGTYPE handle_hupterm(int signal)
{
    die(_("Received SIGHUP or SIGTERM\n"));
}

/* Handler for SIGTSTP (suspend). */
RETSIGTYPE do_suspend(int signal)
{
#ifndef DISABLE_MOUSE
    /* Turn mouse support off. */
    disable_mouse_support();
#endif

    /* Move the cursor to the last line of the screen. */
    move(LINES - 1, 0);
    endwin();

    /* Display our helpful message. */
    printf(_("Use \"fg\" to return to nano.\n"));
    fflush(stdout);

    /* Restore the old terminal settings. */
    tcsetattr(0, TCSANOW, &oldterm);

    /* Trap SIGHUP and SIGTERM so we can properly deal with them while
     * suspended. */
    act.sa_handler = handle_hupterm;
    sigaction(SIGHUP, &act, NULL);
    sigaction(SIGTERM, &act, NULL);

    /* Do what mutt does: send ourselves a SIGSTOP. */
    kill(0, SIGSTOP);
}

/* The version of above function that is bound to a key. */
void do_suspend_void(void)
{
    if (ISSET(SUSPEND))
	do_suspend(0);
    else {
	statusbar(_("Suspension is not enabled"));
	beep();
    }
}

/* Handler for SIGCONT (continue after suspend). */
RETSIGTYPE do_continue(int signal)
{
#ifndef DISABLE_MOUSE
    /* Turn mouse support back on if it was on before. */
    if (ISSET(USE_MOUSE))
	enable_mouse_support();
#endif

#ifndef NANO_TINY
    /* Perhaps the user resized the window while we slept.  Handle it,
     * and restore the terminal to its previous state and update the
     * screen in the process. */
    handle_sigwinch(0);
#else
    /* Restore the terminal to its previous state. */
    terminal_init();

    /* Redraw the contents of the windows that need it. */
    blank_statusbar();
    wnoutrefresh(bottomwin);
    total_refresh();
#endif
}

#ifndef NANO_TINY
/* Handler for SIGWINCH (window size change). */
RETSIGTYPE handle_sigwinch(int signal)
{
    /* Let the input routine know that a SIGWINCH has occurred. */
    sigwinch_counter++;
}

/* Reinitialize and redraw the screen completely. */
void regenerate_screen(void)
{
    const char *tty = ttyname(0);
    int fd, result = 0;
    struct winsize win;

    if (tty == NULL)
	return;
    fd = open(tty, O_RDWR);
    if (fd == -1)
	return;
    result = ioctl(fd, TIOCGWINSZ, &win);
    close(fd);
    if (result == -1)
	return;

    /* We could check whether the COLS or LINES changed, and return
     * otherwise.  However, COLS and LINES are curses global variables,
     * and in some cases curses has already updated them.  But not in
     * all cases.  Argh. */
#ifdef REDEFINING_MACROS_OK
    COLS = win.ws_col;
    LINES = win.ws_row;
#endif

#ifdef USE_SLANG
    /* Slang curses emulation brain damage, part 1: If we just do what
     * curses does here, it'll only work properly if the resize made the
     * window smaller.  Do what mutt does: Leave and immediately reenter
     * Slang screen management mode. */
    SLsmg_reset_smg();
    SLsmg_init_smg();
#else
    /* Do the equivalent of what Minimum Profit does: Leave and
     * immediately reenter curses mode. */
    endwin();
    doupdate();
#endif

    /* Restore the terminal to its previous state. */
    terminal_init();

    /* Do the equivalent of what both mutt and Minimum Profit do:
     * Reinitialize all the windows based on the new screen
     * dimensions. */
    window_init();

    /* Redraw the contents of the windows that need it. */
    total_refresh();
}

/* If allow is FALSE, block any SIGWINCH signal.  If allow is TRUE,
 * unblock SIGWINCH so any pending ones can be dealt with. */
void allow_sigwinch(bool allow)
{
    sigset_t winch;

    sigemptyset(&winch);
    sigaddset(&winch, SIGWINCH);
    sigprocmask(allow ? SIG_UNBLOCK : SIG_BLOCK, &winch, NULL);
}
#endif /* !NANO_TINY */

#ifndef NANO_TINY
/* Handle the global toggle specified in flag. */
void do_toggle(int flag)
{
    bool enabled;

    if (ISSET(RESTRICTED) && (flag == SUSPEND || flag == MULTIBUFFER ||
			flag == BACKUP_FILE || flag == NO_COLOR_SYNTAX)) {
	show_restricted_warning();
	return;
    }

    TOGGLE(flag);

    switch (flag) {
#ifndef DISABLE_MOUSE
	case USE_MOUSE:
	    mouse_init();
	    break;
#endif
	case MORE_SPACE:
	case NO_HELP:
	    window_init();
	    total_refresh();
	    break;
	case SUSPEND:
	    signal_init();
	    break;
	case WHITESPACE_DISPLAY:
	    titlebar(NULL);
	    edit_refresh();
	    break;
#ifndef DISABLE_COLOR
	case NO_COLOR_SYNTAX:
#endif
	case SOFTWRAP:
	    edit_refresh();
	    break;
    }

    enabled = ISSET(flag);

    if (flag == NO_HELP
#ifndef DISABLE_WRAPPING
	|| flag == NO_WRAP
#endif
#ifndef DISABLE_COLOR
	|| flag == NO_COLOR_SYNTAX
#endif
	)
	enabled = !enabled;

    statusbar("%s %s", _(flagtostr(flag)), enabled ? _("enabled") : _("disabled"));
}
#endif /* !NANO_TINY */

/* Bleh. */
void do_toggle_void(void)
{
    ;
}

/* Disable extended input and output processing in our terminal
 * settings. */
void disable_extended_io(void)
{
    struct termios term;

    tcgetattr(0, &term);
    term.c_lflag &= ~IEXTEN;
    term.c_oflag &= ~OPOST;
    tcsetattr(0, TCSANOW, &term);
}

/* Disable interpretation of the special control keys in our terminal
 * settings. */
void disable_signals(void)
{
    struct termios term;

    tcgetattr(0, &term);
    term.c_lflag &= ~ISIG;
    tcsetattr(0, TCSANOW, &term);
}

#ifndef NANO_TINY
/* Enable interpretation of the special control keys in our terminal
 * settings. */
void enable_signals(void)
{
    struct termios term;

    tcgetattr(0, &term);
    term.c_lflag |= ISIG;
    tcsetattr(0, TCSANOW, &term);
}
#endif

/* Disable interpretation of the flow control characters in our terminal
 * settings. */
void disable_flow_control(void)
{
    struct termios term;

    tcgetattr(0, &term);
    term.c_iflag &= ~IXON;
    tcsetattr(0, TCSANOW, &term);
}

/* Enable interpretation of the flow control characters in our terminal
 * settings. */
void enable_flow_control(void)
{
    struct termios term;

    tcgetattr(0, &term);
    term.c_iflag |= IXON;
    tcsetattr(0, TCSANOW, &term);
}

/* Set up the terminal state.  Put the terminal in raw mode (read one
 * character at a time, disable the special control keys, and disable
 * the flow control characters), disable translation of carriage return
 * (^M) into newline (^J) so that we can tell the difference between the
 * Enter key and Ctrl-J, and disable echoing of characters as they're
 * typed.  Finally, disable extended input and output processing, and,
 * if we're not in preserve mode, reenable interpretation of the flow
 * control characters. */
void terminal_init(void)
{
#ifdef USE_SLANG
    /* Slang curses emulation brain damage, part 2: Slang doesn't
     * implement raw(), nonl(), or noecho() properly, so there's no way
     * to properly reinitialize the terminal using them.  We have to
     * disable the special control keys and interpretation of the flow
     * control characters using termios, save the terminal state after
     * the first call, and restore it on subsequent calls. */
    static struct termios newterm;
    static bool newterm_set = FALSE;

    if (!newterm_set) {
#endif

	raw();
	nonl();
	noecho();
	disable_extended_io();
	if (ISSET(PRESERVE))
	    enable_flow_control();

	disable_signals();
#ifdef USE_SLANG
	if (!ISSET(PRESERVE))
	    disable_flow_control();

	tcgetattr(0, &newterm);
	newterm_set = TRUE;
    } else
	tcsetattr(0, TCSANOW, &newterm);
#endif
}

/* Read in a character, interpret it as a shortcut or toggle if
 * necessary, and return it.
 * If allow_funcs is FALSE, don't actually run any functions associated
 * with shortcut keys. */
int do_input(bool allow_funcs)
{
    int input;
	/* The character we read in. */
    static int *kbinput = NULL;
	/* The input buffer. */
    static size_t kbinput_len = 0;
	/* The length of the input buffer. */
    bool preserve = FALSE;
	/* Preserve the contents of the cutbuffer? */
    const sc *s;
    bool have_shortcut;

    /* Read in a character. */
    input = get_kbinput(edit);

#ifndef NANO_TINY
    if (input == KEY_WINCH)
	return KEY_WINCH;
#endif

#ifndef DISABLE_MOUSE
    if (func_key && input == KEY_MOUSE) {
	/* We received a mouse click. */
	if (do_mouse() == 1)
	    /* The click was on a shortcut -- read in the character
	     * that it was converted into. */
	    input = get_kbinput(edit);
	else
	    /* The click was invalid or has been handled -- get out. */
	    return ERR;
    }
#endif

    /* Check for a shortcut in the main list. */
    s = get_shortcut(&input);

    /* If we got a shortcut from the main list, or a "universal"
     * edit window shortcut, set have_shortcut to TRUE. */
    have_shortcut = (s != NULL);

    /* If we got a non-high-bit control key, a meta key sequence, or a
     * function key, and it's not a shortcut or toggle, throw it out. */
    if (!have_shortcut) {
	if (is_ascii_cntrl_char(input) || meta_key || func_key) {
	    statusbar(_("Unknown Command"));
	    beep();
	    meta_key = FALSE;
	    func_key = FALSE;
	    input = ERR;
	}
    }

    if (allow_funcs) {
	/* If we got a character, and it isn't a shortcut or toggle,
	 * it's a normal text character.  Display the warning if we're
	 * in view mode, or add the character to the input buffer if
	 * we're not. */
	if (input != ERR && !have_shortcut) {
	    if (ISSET(VIEW_MODE))
		print_view_warning();
	    else {
		kbinput_len++;
		kbinput = (int *)nrealloc(kbinput, kbinput_len *
			sizeof(int));
		kbinput[kbinput_len - 1] = input;
	    }
	}

	/* If we got a shortcut or toggle, or if there aren't any other
	 * characters waiting after the one we read in, we need to
	 * output all the characters in the input buffer if it isn't
	 * empty.  Note that it should be empty if we're in view
	 * mode. */
	if (have_shortcut || get_key_buffer_len() == 0) {
#ifndef DISABLE_WRAPPING
	    /* If we got a shortcut or toggle, and it's not the shortcut
	     * for verbatim input, turn off prepending of wrapped text. */
	    if (have_shortcut && s->scfunc != do_verbatim_input)
		wrap_reset();
#endif

	    if (kbinput != NULL) {
		/* Display all the characters in the input buffer at
		 * once, filtering out control characters. */
		char *output = charalloc(kbinput_len + 1);
		size_t i;

		for (i = 0; i < kbinput_len; i++)
		    output[i] = (char)kbinput[i];
		output[i] = '\0';

		do_output(output, kbinput_len, FALSE);

		free(output);

		/* Empty the input buffer. */
		kbinput_len = 0;
		free(kbinput);
		kbinput = NULL;
	    }
	}

	if (have_shortcut) {
	    const subnfunc *f = sctofunc(s);
	    /* If the function associated with this shortcut is
	     * cutting or copying text, remember this. */
	    if (s->scfunc == do_cut_text_void
#ifndef NANO_TINY
		|| s->scfunc == do_copy_text || s->scfunc == do_cut_till_eof
#endif
		)
		preserve = TRUE;

	    if (s->scfunc == NULL) {
		statusbar("Internal error: shortcut without function!");
		return ERR;
	    }
	    if (ISSET(VIEW_MODE) && f && !f->viewok)
		print_view_warning();
	    else {
#ifndef NANO_TINY
		if (s->scfunc == do_toggle_void) {
		    do_toggle(s->toggle);
		    if (s->toggle != CUT_TO_END)
			preserve = TRUE;
		} else
#endif
		{
		    /* Execute the function of the shortcut. */
		    s->scfunc();
#ifndef DISABLE_COLOR
		    if (f && !f->viewok)
			reset_multis(openfile->current, FALSE);
#endif
		    if (edit_refresh_needed) {
#ifdef DEBUG
			fprintf(stderr, "running edit_refresh() as edit_refresh_needed is true\n");
#endif
			edit_refresh();
			edit_refresh_needed = FALSE;
		    } else if (s->scfunc == do_delete || s->scfunc == do_backspace)
			update_line(openfile->current, openfile->current_x);
		}
	    }
	}
    }

    /* If we aren't cutting or copying text, and the key wasn't a toggle,
     * blow away the text in the cutbuffer upon the next cutting action. */
    if (!preserve)
	cutbuffer_reset();

    return input;
}

void xon_complaint(void)
{
    statusbar(_("XON ignored, mumble mumble"));
}

void xoff_complaint(void)
{
    statusbar(_("XOFF ignored, mumble mumble"));
}


#ifndef DISABLE_MOUSE
/* Handle a mouse click on the edit window or the shortcut list. */
int do_mouse(void)
{
    int mouse_x, mouse_y;
    int retval = get_mouseinput(&mouse_x, &mouse_y, TRUE);

    if (retval != 0)
	/* The click is wrong or already handled. */
	return retval;

    /* We can click on the edit window to move the cursor. */
    if (wmouse_trafo(edit, &mouse_y, &mouse_x, FALSE)) {
	bool sameline;
	    /* Did they click on the line with the cursor?  If they
	     * clicked on the cursor, we set the mark. */
	filestruct *current_save = openfile->current;
#ifndef NANO_TINY
	size_t current_x_save = openfile->current_x;
#endif
	size_t pww_save = openfile->placewewant;

	sameline = (mouse_y == openfile->current_y);

#ifdef DEBUG
	fprintf(stderr, "mouse_y = %d, current_y = %ld\n", mouse_y, (long)openfile->current_y);
#endif

#ifndef NANO_TINY
	if (ISSET(SOFTWRAP)) {
	    size_t i = 0;
	    for (openfile->current = openfile->edittop;
		 openfile->current->next && i < mouse_y;
		 openfile->current = openfile->current->next, i++) {
		openfile->current_y = i;
		i += strlenpt(openfile->current->data) / COLS;
	    }
#ifdef DEBUG
	    fprintf(stderr, "do_mouse(): moving to current_y = %ld, index i = %lu\n",
			(long)openfile->current_y, (unsigned long)i);
	    fprintf(stderr, "            openfile->current->data = \"%s\"\n", openfile->current->data);
#endif

	    if (i > mouse_y) {
		openfile->current = openfile->current->prev;
		openfile->current_x = actual_x(openfile->current->data, mouse_x + (mouse_y - openfile->current_y) * COLS);
#ifdef DEBUG
		fprintf(stderr, "do_mouse(): i > mouse_y, mouse_x = %d, current_x to = %lu\n",
			mouse_x, (unsigned long)openfile->current_x);
#endif
	    } else {
		openfile->current_x = actual_x(openfile->current->data, mouse_x);
#ifdef DEBUG
		fprintf(stderr, "do_mouse(): i <= mouse_y, mouse_x = %d, setting current_x to = %lu\n",
			mouse_x, (unsigned long)openfile->current_x);
#endif
	    }
	} else
#endif /* NANO_TINY */
	{
	    /* Move to where the click occurred. */
	    for (; openfile->current_y < mouse_y && openfile->current !=
		   openfile->filebot; openfile->current_y++)
		openfile->current = openfile->current->next;
	    for (; openfile->current_y > mouse_y && openfile->current !=
		   openfile->fileage; openfile->current_y--)
		openfile->current = openfile->current->prev;

	    openfile->current_x = actual_x(openfile->current->data,
		get_page_start(xplustabs()) + mouse_x);
	}

	openfile->placewewant = xplustabs();

#ifndef NANO_TINY
	/* Clicking where the cursor is toggles the mark, as does
	 * clicking beyond the line length with the cursor at the end of
	 * the line. */
	if (sameline && openfile->current_x == current_x_save)
	    do_mark();
	else
#endif
	    /* The cursor moved; clean the cutbuffer on the next cut. */
	    cutbuffer_reset();

	edit_redraw(current_save, pww_save);
    }

    /* No more handling is needed. */
    return 2;
}
#endif /* !DISABLE_MOUSE */

/* The user typed output_len multibyte characters.  Add them to the edit
 * buffer, filtering out all ASCII control characters if allow_cntrls is
 * TRUE. */
void do_output(char *output, size_t output_len, bool allow_cntrls)
{
    size_t current_len, i = 0;
#ifndef NANO_TINY
    size_t orig_lenpt = 0;
#endif

    char *char_buf = charalloc(mb_cur_max());
    int char_buf_len;

    assert(openfile->current != NULL && openfile->current->data != NULL);

    current_len = strlen(openfile->current->data);

#ifndef NANO_TINY
    if (ISSET(SOFTWRAP))
	orig_lenpt = strlenpt(openfile->current->data);
#endif

    while (i < output_len) {
	/* If allow_cntrls is TRUE, convert nulls and newlines properly. */
	if (allow_cntrls) {
	    /* Null to newline, if needed. */
	    if (output[i] == '\0')
		output[i] = '\n';
	    /* Newline to Enter, if needed. */
	    else if (output[i] == '\n') {
		do_enter();
		i++;
		continue;
	    }
	}

	/* Interpret the next multibyte character. */
	char_buf_len = parse_mbchar(output + i, char_buf, NULL);

	i += char_buf_len;

	/* If allow_cntrls is FALSE, filter out an ASCII control character. */
	if (!allow_cntrls && is_ascii_cntrl_char(*(output + i -	char_buf_len)))
	    continue;

	/* If the NO_NEWLINES flag isn't set, when a character is
	 * added to the magicline, it means we need a new magicline. */
	if (!ISSET(NO_NEWLINES) && openfile->filebot == openfile->current)
	    new_magicline();

	/* More dangerousness fun =) */
	openfile->current->data = charealloc(openfile->current->data,
					current_len + (char_buf_len * 2));

	assert(openfile->current_x <= current_len);

	charmove(openfile->current->data + openfile->current_x + char_buf_len,
			openfile->current->data + openfile->current_x,
			current_len - openfile->current_x + char_buf_len);
	strncpy(openfile->current->data + openfile->current_x, char_buf,
		char_buf_len);
	current_len += char_buf_len;
	openfile->totsize++;
	set_modified();

#ifndef NANO_TINY
	add_undo(ADD);

	/* Note that current_x has not yet been incremented. */
	if (openfile->mark_set && openfile->current == openfile->mark_begin &&
		openfile->current_x < openfile->mark_begin_x)
	    openfile->mark_begin_x += char_buf_len;
#endif

	openfile->current_x += char_buf_len;

#ifndef NANO_TINY
	update_undo(ADD);
#endif

#ifndef DISABLE_WRAPPING
	/* If we're wrapping text, we need to call edit_refresh(). */
	if (!ISSET(NO_WRAP))
	    if (do_wrap(openfile->current))
		edit_refresh_needed = TRUE;
#endif
    }

#ifndef NANO_TINY
    /* Well, we might also need a full refresh if we've changed the
     * line length to be a new multiple of COLS. */
    if (ISSET(SOFTWRAP) && edit_refresh_needed == FALSE)
	if (strlenpt(openfile->current->data) / COLS != orig_lenpt / COLS)
	    edit_refresh_needed = TRUE;
#endif

    free(char_buf);

    openfile->placewewant = xplustabs();

#ifndef DISABLE_COLOR
    reset_multis(openfile->current, FALSE);
#endif

    if (edit_refresh_needed == TRUE) {
	edit_refresh();
	edit_refresh_needed = FALSE;
    } else
	update_line(openfile->current, openfile->current_x);
}

int main(int argc, char **argv)
{
    int optchr;
    ssize_t startline = 0, startcol = 0;
	/* Target line and column when specified on the command line. */
#ifndef DISABLE_WRAPJUSTIFY
    bool fill_used = FALSE;
	/* Was the fill option used on the command line? */
    bool forced_wrapping = FALSE;
	/* Should long lines be automatically hard wrapped? */
#endif
#ifndef DISABLE_MULTIBUFFER
    bool old_multibuffer;
	/* The old value of the multibuffer option, restored after we
	 * load all files on the command line. */
#endif
#ifdef HAVE_GETOPT_LONG
    const struct option long_options[] = {
	{"boldtext", 0, NULL, 'D'},
#ifndef DISABLE_MULTIBUFFER
	{"multibuffer", 0, NULL, 'F'},
#endif
#ifndef DISABLE_NANORC
	{"ignorercfiles", 0, NULL, 'I'},
#endif
	{"rebindkeypad", 0, NULL, 'K'},
	{"nonewlines", 0, NULL, 'L'},
	{"morespace", 0, NULL, 'O'},
#ifndef DISABLE_JUSTIFY
	{"quotestr", 1, NULL, 'Q'},
#endif
	{"restricted", 0, NULL, 'R'},
	{"tabsize", 1, NULL, 'T'},
	{"version", 0, NULL, 'V'},
#ifndef DISABLE_COLOR
	{"syntax", 1, NULL, 'Y'},
#endif
	{"constantshow", 0, NULL, 'c'},
	{"rebinddelete", 0, NULL, 'd'},
	{"help", 0, NULL, 'h'},
#ifndef DISABLE_MOUSE
	{"mouse", 0, NULL, 'm'},
#endif
	{"noread", 0, NULL, 'n'},
#ifndef DISABLE_OPERATINGDIR
	{"operatingdir", 1, NULL, 'o'},
#endif
	{"preserve", 0, NULL, 'p'},
	{"quiet", 0, NULL, 'q'},
#ifndef DISABLE_WRAPJUSTIFY
	{"fill", 1, NULL, 'r'},
#endif
#ifndef DISABLE_SPELLER
	{"speller", 1, NULL, 's'},
#endif
	{"tempfile", 0, NULL, 't'},
	{"view", 0, NULL, 'v'},
#ifndef DISABLE_WRAPPING
	{"nowrap", 0, NULL, 'w'},
#endif
	{"nohelp", 0, NULL, 'x'},
	{"suspend", 0, NULL, 'z'},
#ifndef NANO_TINY
	{"smarthome", 0, NULL, 'A'},
	{"backup", 0, NULL, 'B'},
	{"backupdir", 1, NULL, 'C'},
	{"tabstospaces", 0, NULL, 'E'},
	{"locking", 0, NULL, 'G'},
	{"historylog", 0, NULL, 'H'},
	{"noconvert", 0, NULL, 'N'},
	{"poslog", 0, NULL, 'P'},  /* deprecated form, remove in 2018 */
	{"positionlog", 0, NULL, 'P'},
	{"smooth", 0, NULL, 'S'},
	{"quickblank", 0, NULL, 'U'},
	{"wordbounds", 0, NULL, 'W'},
	{"autoindent", 0, NULL, 'i'},
	{"cut", 0, NULL, 'k'},
	{"unix", 0, NULL, 'u'},
	{"softwrap", 0, NULL, '$'},
#endif
	{NULL, 0, NULL, 0}
    };
#endif

#ifdef ENABLE_UTF8
    {
	/* If the locale set exists and uses UTF-8, we should use
	 * UTF-8. */
	char *locale = setlocale(LC_ALL, "");

	if (locale != NULL && (strcmp(nl_langinfo(CODESET),
		"UTF-8") == 0)) {
#ifdef USE_SLANG
	    SLutf8_enable(1);
#endif
	    utf8_init();
	}
    }
#else
    setlocale(LC_ALL, "");
#endif

#ifdef ENABLE_NLS
    bindtextdomain(PACKAGE, LOCALEDIR);
    textdomain(PACKAGE);
#endif

#if defined(DISABLE_NANORC) && defined(DISABLE_ROOTWRAPPING)
    /* If we don't have rcfile support, --disable-wrapping-as-root is
     * used, and we're root, turn wrapping off. */
    if (geteuid() == NANO_ROOT_UID)
	SET(NO_WRAP);
#endif

    while ((optchr =
#ifdef HAVE_GETOPT_LONG
	getopt_long(argc, argv,
		"ABC:DEFGHIKLNOPQ:RST:UVWY:abcdefghijklmno:pqr:s:tuvwxz$",
		long_options, NULL)
#else
	getopt(argc, argv,
		"ABC:DEFGHIKLNOPQ:RST:UVWY:abcdefghijklmno:pqr:s:tuvwxz$")
#endif
		) != -1) {
	switch (optchr) {
	    case 'a':
	    case 'b':
	    case 'e':
	    case 'f':
	    case 'g':
	    case 'j':
		/* Pico compatibility flags. */
		break;
#ifndef NANO_TINY
	    case 'A':
		SET(SMART_HOME);
		break;
	    case 'B':
		SET(BACKUP_FILE);
		break;
	    case 'C':
		backup_dir = mallocstrcpy(backup_dir, optarg);
		break;
#endif
	    case 'D':
		SET(BOLD_TEXT);
		break;
#ifndef NANO_TINY
	    case 'E':
		SET(TABS_TO_SPACES);
		break;
#endif
#ifndef DISABLE_MULTIBUFFER
	    case 'F':
		SET(MULTIBUFFER);
		break;
#endif
#ifndef NANO_TINY
	    case 'G':
		SET(LOCKING);
		break;
#endif
#ifndef DISABLE_HISTORIES
	    case 'H':
		SET(HISTORYLOG);
		break;
#endif
#ifndef DISABLE_NANORC
	    case 'I':
		no_rcfiles = TRUE;
		break;
#endif
	    case 'K':
		SET(REBIND_KEYPAD);
		break;
	    case 'L':
		SET(NO_NEWLINES);
		break;
#ifndef NANO_TINY
	    case 'N':
		SET(NO_CONVERT);
		break;
#endif
	    case 'O':
		SET(MORE_SPACE);
		break;
#ifndef DISABLE_HISTORIES
	    case 'P':
		SET(POS_HISTORY);
		break;
#endif
#ifndef DISABLE_JUSTIFY
	    case 'Q':
		quotestr = mallocstrcpy(quotestr, optarg);
		break;
#endif
	    case 'R':
		SET(RESTRICTED);
		break;
#ifndef NANO_TINY
	    case 'S':
		SET(SMOOTH_SCROLL);
		break;
#endif
	    case 'T':
		if (!parse_num(optarg, &tabsize) || tabsize <= 0) {
		    fprintf(stderr, _("Requested tab size \"%s\" is invalid"), optarg);
		    fprintf(stderr, "\n");
		    exit(1);
		}
		break;
#ifndef NANO_TINY
	    case 'U':
		SET(QUICK_BLANK);
		break;
#endif
	    case 'V':
		version();
		exit(0);
#ifndef NANO_TINY
	    case 'W':
		SET(WORD_BOUNDS);
		break;
#endif
#ifndef DISABLE_COLOR
	    case 'Y':
		syntaxstr = mallocstrcpy(syntaxstr, optarg);
		break;
#endif
	    case 'c':
		SET(CONST_UPDATE);
		break;
	    case 'd':
		SET(REBIND_DELETE);
		break;
#ifndef NANO_TINY
	    case 'i':
		SET(AUTOINDENT);
		break;
	    case 'k':
		SET(CUT_TO_END);
		break;
#endif
#ifndef DISABLE_MOUSE
	    case 'm':
		SET(USE_MOUSE);
		break;
#endif
	    case 'n':
		SET(NOREAD_MODE);
		break;
#ifndef DISABLE_OPERATINGDIR
	    case 'o':
		operating_dir = mallocstrcpy(operating_dir, optarg);
		break;
#endif
	    case 'p':
		SET(PRESERVE);
		break;
#ifndef DISABLE_NANORC
	    case 'q':
		SET(QUIET);
		break;
#endif
#ifndef DISABLE_WRAPJUSTIFY
	    case 'r':
		if (!parse_num(optarg, &wrap_at)) {
		    fprintf(stderr, _("Requested fill size \"%s\" is invalid"), optarg);
		    fprintf(stderr, "\n");
		    exit(1);
		}
		fill_used = TRUE;
		forced_wrapping = TRUE;
		break;
#endif
#ifndef DISABLE_SPELLER
	    case 's':
		alt_speller = mallocstrcpy(alt_speller, optarg);
		break;
#endif
	    case 't':
		SET(TEMP_FILE);
		break;
#ifndef NANO_TINY
	    case 'u':
		SET(MAKE_IT_UNIX);
		break;
#endif
	    case 'v':
		SET(VIEW_MODE);
		break;
#ifndef DISABLE_WRAPPING
	    case 'w':
		SET(NO_WRAP);
		/* If both --fill and --nowrap are given on the
		 * command line, the last given option wins. */
		forced_wrapping = FALSE;
		break;
#endif
	    case 'x':
		SET(NO_HELP);
		break;
	    case 'z':
		SET(SUSPEND);
		break;
#ifndef NANO_TINY
	    case '$':
		SET(SOFTWRAP);
		break;
#endif
	    case 'h':
		usage();
		exit(0);
	    default:
		printf(_("Type '%s -h' for a list of available options.\n"), argv[0]);
		exit(1);
	}
    }

    /* If the executable filename starts with 'r', enable restricted
     * mode. */
    if (*(tail(argv[0])) == 'r')
	SET(RESTRICTED);

    /* If we're using restricted mode, disable suspending, backups,
     * rcfiles, and history files, since they all would allow reading
     * from or writing to files not specified on the command line. */
    if (ISSET(RESTRICTED)) {
	UNSET(SUSPEND);
	UNSET(BACKUP_FILE);
#ifndef DISABLE_NANORC
	no_rcfiles = TRUE;
	UNSET(HISTORYLOG);
	UNSET(POS_HISTORY);
#endif
    }

    /* Set up the function and shortcut lists.  This needs to be done
     * before reading the rcfile, to be able to rebind/unbind keys. */
    shortcut_init();

/* We've read through the command line options.  Now back up the flags
 * and values that are set, and read the rcfile(s).  If the values
 * haven't changed afterward, restore the backed-up values. */
#ifndef DISABLE_NANORC
    if (!no_rcfiles) {
#ifndef DISABLE_OPERATINGDIR
	char *operating_dir_cpy = operating_dir;
#endif
#ifndef DISABLE_WRAPJUSTIFY
	ssize_t wrap_at_cpy = wrap_at;
#endif
#ifndef NANO_TINY
	char *backup_dir_cpy = backup_dir;
#endif
#ifndef DISABLE_JUSTIFY
	char *quotestr_cpy = quotestr;
#endif
#ifndef DISABLE_SPELLER
	char *alt_speller_cpy = alt_speller;
#endif
	ssize_t tabsize_cpy = tabsize;
	unsigned flags_cpy[sizeof(flags) / sizeof(flags[0])];
	size_t i;

	memcpy(flags_cpy, flags, sizeof(flags_cpy));

#ifndef DISABLE_OPERATINGDIR
	operating_dir = NULL;
#endif
#ifndef NANO_TINY
	backup_dir = NULL;
#endif
#ifndef DISABLE_JUSTIFY
	quotestr = NULL;
#endif
#ifndef DISABLE_SPELLER
	alt_speller = NULL;
#endif

	do_rcfile();

#ifdef DEBUG
	fprintf(stderr, "After rebinding keys...\n");
	print_sclist();
#endif

#ifndef DISABLE_OPERATINGDIR
	if (operating_dir_cpy != NULL) {
	    free(operating_dir);
	    operating_dir = operating_dir_cpy;
	}
#endif
#ifndef DISABLE_WRAPJUSTIFY
	if (fill_used)
	    wrap_at = wrap_at_cpy;
#endif
#ifndef NANO_TINY
	if (backup_dir_cpy != NULL) {
	    free(backup_dir);
	    backup_dir = backup_dir_cpy;
	}
#endif
#ifndef DISABLE_JUSTIFY
	if (quotestr_cpy != NULL) {
	    free(quotestr);
	    quotestr = quotestr_cpy;
	}
#endif
#ifndef DISABLE_SPELLER
	if (alt_speller_cpy != NULL) {
	    free(alt_speller);
	    alt_speller = alt_speller_cpy;
	}
#endif
	if (tabsize_cpy != -1)
	    tabsize = tabsize_cpy;

	for (i = 0; i < sizeof(flags) / sizeof(flags[0]); i++)
	    flags[i] |= flags_cpy[i];
    }
#ifdef DISABLE_ROOTWRAPPING
    /* If we don't have any rcfiles, --disable-wrapping-as-root is used,
     * and we're root, turn wrapping off. */
    else if (geteuid() == NANO_ROOT_UID)
	SET(NO_WRAP);
#endif
#endif /* !DISABLE_NANORC */

#ifndef DISABLE_WRAPPING
    /* Override a "set nowrap" in an rcfile (or a --disable-wrapping-as-root)
     * if --fill was given on the command line and not undone by --nowrap. */
    if (forced_wrapping)
	UNSET(NO_WRAP);
#endif

    /* If we're using bold text instead of reverse video text, set it up
     * now. */
    if (ISSET(BOLD_TEXT))
	hilite_attribute = A_BOLD;

#ifndef DISABLE_HISTORIES
    /* Set up the search/replace history. */
    history_init();
    /* Verify that the home directory and ~/.nano subdir exist. */
    if (ISSET(HISTORYLOG) || ISSET(POS_HISTORY)) {
	get_homedir();
	if (homedir == NULL || check_dotnano() == 0) {
	    UNSET(HISTORYLOG);
	    UNSET(POS_HISTORY);
	}
    }
    if (ISSET(HISTORYLOG))
	load_history();
    if (ISSET(POS_HISTORY))
	load_poshistory();
#endif /* !DISABLE_HISTORIES */

#ifndef NANO_TINY
    /* Set up the backup directory (unless we're using restricted mode,
     * in which case backups are disabled, since they would allow
     * reading from or writing to files not specified on the command
     * line).  This entails making sure it exists and is a directory, so
     * that backup files will be saved there. */
    if (!ISSET(RESTRICTED))
	init_backup_dir();
#endif

#ifndef DISABLE_OPERATINGDIR
    /* Set up the operating directory.  This entails chdir()ing there,
     * so that file reads and writes will be based there. */
    init_operating_dir();
#endif

#ifndef DISABLE_JUSTIFY
    /* If punct wasn't specified, set its default value. */
    if (punct == NULL)
	punct = mallocstrcpy(NULL, "!.?");

    /* If brackets wasn't specified, set its default value. */
    if (brackets == NULL)
	brackets = mallocstrcpy(NULL, "\"')>]}");

    /* If quotestr wasn't specified, set its default value. */
    if (quotestr == NULL)
	quotestr = mallocstrcpy(NULL,
#ifdef HAVE_REGEX_H
		"^([ \t]*[#:>|}])+"
#else
		"> "
#endif
		);
#ifdef HAVE_REGEX_H
    quoterc = regcomp(&quotereg, quotestr, REG_EXTENDED);

    if (quoterc == 0) {
	/* We no longer need quotestr, just quotereg. */
	free(quotestr);
	quotestr = NULL;
    } else {
	size_t size = regerror(quoterc, &quotereg, NULL, 0);

	quoteerr = charalloc(size);
	regerror(quoterc, &quotereg, quoteerr, size);
    }
#else
    quotelen = strlen(quotestr);
#endif /* !HAVE_REGEX_H */
#endif /* !DISABLE_JUSTIFY */

#ifndef DISABLE_SPELLER
    /* If we don't have an alternative spell checker after reading the
     * command line and/or rcfile(s), check $SPELL for one, as Pico
     * does (unless we're using restricted mode, in which case spell
     * checking is disabled, since it would allow reading from or
     * writing to files not specified on the command line). */
    if (!ISSET(RESTRICTED) && alt_speller == NULL) {
	char *spellenv = getenv("SPELL");
	if (spellenv != NULL)
	    alt_speller = mallocstrcpy(NULL, spellenv);
    }
#endif

#ifndef NANO_TINY
    /* If matchbrackets wasn't specified, set its default value. */
    if (matchbrackets == NULL)
	matchbrackets = mallocstrcpy(NULL, "(<[{)>]}");

    /* If whitespace wasn't specified, set its default value.  If we're
     * using UTF-8, it's Unicode 00BB (Right-Pointing Double Angle
     * Quotation Mark) and Unicode 00B7 (Middle Dot).  Otherwise, it's
     * ">" and ".". */
    if (whitespace == NULL) {
#ifdef ENABLE_UTF8
	if (using_utf8()) {
	    whitespace = mallocstrcpy(NULL, "\xC2\xBB\xC2\xB7");
	    whitespace_len[0] = 2;
	    whitespace_len[1] = 2;
	} else
#endif
	{
	    whitespace = mallocstrcpy(NULL, ">.");
	    whitespace_len[0] = 1;
	    whitespace_len[1] = 1;
	}
    }
#endif /* !NANO_TINY */

    /* Initialize the search and replace strings. */
    last_search = mallocstrcpy(NULL, "");
    last_replace = mallocstrcpy(NULL, "");

    /* If tabsize wasn't specified, set its default value. */
    if (tabsize == -1)
	tabsize = WIDTH_OF_TAB;

    /* Back up the old terminal settings so that they can be restored. */
    tcgetattr(0, &oldterm);

    /* Initialize curses mode.  If this fails, get out. */
    if (initscr() == NULL)
	exit(1);

    /* Set up the terminal state. */
    terminal_init();

#ifdef DEBUG
    fprintf(stderr, "Main: set up windows\n");
#endif

    /* Initialize all the windows based on the current screen
     * dimensions. */
    window_init();

    /* Set up the signal handlers. */
    signal_init();

#ifndef DISABLE_MOUSE
    /* Initialize mouse support. */
    mouse_init();
#endif

#ifndef DISABLE_COLOR
    set_colorpairs();
#else
    interface_color_pair[TITLE_BAR].pairnum = hilite_attribute;
    interface_color_pair[STATUS_BAR].pairnum = hilite_attribute;
    interface_color_pair[KEY_COMBO].pairnum = hilite_attribute;
    interface_color_pair[FUNCTION_TAG].pairnum = A_NORMAL;
    interface_color_pair[TITLE_BAR].bright = FALSE;
    interface_color_pair[STATUS_BAR].bright = FALSE;
    interface_color_pair[KEY_COMBO].bright = FALSE;
    interface_color_pair[FUNCTION_TAG].bright = FALSE;
#endif

#if !defined(NANO_TINY) && defined(HAVE_KEY_DEFINED)
    const char *keyvalue;
    /* Ask ncurses for the key codes for Control+Left and Control+Right. */
    keyvalue = tigetstr("kLFT5");
    if (keyvalue != 0 && keyvalue != (char *)-1)
	controlleft = key_defined(keyvalue);
    keyvalue = tigetstr("kRIT5");
    if (keyvalue != 0 && keyvalue != (char *)-1)
	controlright = key_defined(keyvalue);
#endif

#ifdef DEBUG
    fprintf(stderr, "Main: open file\n");
#endif

    /* If there's a +LINE or +LINE,COLUMN flag here, it is the first
     * non-option argument, and it is followed by at least one other
     * argument, the filename it applies to. */
    if (0 < optind && optind < argc - 1 && argv[optind][0] == '+') {
	parse_line_column(&argv[optind][1], &startline, &startcol);
	optind++;
    }

    if (optind < argc && !strcmp(argv[optind], "-")) {
	stdin_pager();
	set_modified();
	optind++;
    }

#ifndef DISABLE_MULTIBUFFER
    old_multibuffer = ISSET(MULTIBUFFER);
    SET(MULTIBUFFER);

    /* Read all the files after the first one on the command line into
     * new buffers. */
    {
	int i = optind + 1;
	ssize_t iline = 0, icol = 0;

	for (; i < argc; i++) {
	    /* If there's a +LINE or +LINE,COLUMN flag here, it is followed
	     * by at least one other argument: the filename it applies to. */
	    if (i < argc - 1 && argv[i][0] == '+')
		parse_line_column(&argv[i][1], &iline, &icol);
	    else {
		/* If opening fails, don't try to position the cursor. */
		if (!open_buffer(argv[i], FALSE))
		    continue;

		/* If a position was given on the command line, go there. */
		if (iline > 0 || icol > 0) {
		    do_gotolinecolumn(iline, icol, FALSE, FALSE);
		    iline = 0;
		    icol = 0;
		}
#ifndef DISABLE_HISTORIES
		else if (ISSET(POS_HISTORY)) {
		    ssize_t savedposline, savedposcol;
		    /* If edited before, restore the last cursor position. */
		    if (check_poshistory(argv[i], &savedposline, &savedposcol))
			do_gotolinecolumn(savedposline, savedposcol,
						FALSE, FALSE);
		}
#endif
	    }
	}
    }
#endif /* !DISABLE_MULTIBUFFER */

    /* Now read the first file on the command line into a new buffer. */
    if (optind < argc)
	open_buffer(argv[optind], FALSE);

    /* If all the command-line arguments were invalid files like directories,
     * or if there were no filenames given, we didn't open any file.  In this
     * case, load a blank buffer.  Also, unset view mode to allow editing. */
    if (openfile == NULL) {
	open_buffer("", FALSE);
	UNSET(VIEW_MODE);
    }

#ifndef DISABLE_MULTIBUFFER
    if (!old_multibuffer)
	UNSET(MULTIBUFFER);
#endif

    /* If a starting position was given on the command line, go there. */
    if (startline > 0 || startcol > 0)
	do_gotolinecolumn(startline, startcol, FALSE, FALSE);
#ifndef DISABLE_HISTORIES
    else if (ISSET(POS_HISTORY)) {
	ssize_t savedposline, savedposcol;
	/* If the file was edited before, restore the last cursor position. */
	if (check_poshistory(argv[optind], &savedposline, &savedposcol))
	    do_gotolinecolumn(savedposline, savedposcol, FALSE, FALSE);
    }
#endif

#ifdef DEBUG
    fprintf(stderr, "Main: bottom win, top win and edit win\n");
#endif

    display_main_list();

    display_buffer();

    while (TRUE) {
	/* Make sure the cursor is in the edit window. */
	reset_cursor();
	wnoutrefresh(edit);

	/* If constant cursor position display is on, and there are no
	 * keys waiting in the input buffer, display the current cursor
	 * position on the statusbar. */
	if (ISSET(CONST_UPDATE) && get_key_buffer_len() == 0)
	    do_cursorpos(TRUE);

	currmenu = MMAIN;
	focusing = FALSE;

	/* Turn the cursor on when waiting for input. */
	curs_set(1);

	/* Read in and interpret characters. */
	do_input(TRUE);
    }

    /* We should never get here. */
    assert(FALSE);
}
