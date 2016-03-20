/* $Id: browser.c 5677 2016-02-25 14:08:47Z bens $ */
/**************************************************************************
 *   browser.c                                                            *
 *                                                                        *
 *   Copyright (C) 2001, 2002, 2003, 2004, 2005, 2006, 2007, 2008, 2009,  *
 *   2010, 2011, 2013, 2014, 2015 Free Software Foundation, Inc.          *
 *                                                                        *
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

#include <stdint.h>
#include <stdio.h>
#include <string.h>
#include <unistd.h>
#include <errno.h>

#ifndef DISABLE_BROWSER

static char **filelist = NULL;
	/* The list of files to display in the file browser. */
static size_t filelist_len = 0;
	/* The number of files in the list. */
static int width = 0;
	/* The number of files that we can display per line. */
static int longest = 0;
	/* The number of columns in the longest filename in the list. */
static size_t selected = 0;
	/* The currently selected filename in the list; zero-based. */
static char *path_save = NULL;
	/* A copy of the current path. */

/* Our main file browser function.  path is the tilde-expanded path we
 * start browsing from. */
char *do_browser(char *path, DIR *dir)
{
    char *retval = NULL;
    int kbinput;
    bool old_const_update = ISSET(CONST_UPDATE);
    char *prev_dir = NULL;
	/* The directory we were in before backing up to "..". */
    char *ans = NULL;
	/* The last answer the user typed at the statusbar prompt. */
    size_t old_selected;
	/* The selected file we had before the current selected file. */
    functionptrtype func;
	/* The function of the key the user typed in. */

    /* Don't show a cursor in the file list. */
    curs_set(0);
    blank_statusbar();
    bottombars(MBROWSER);
    wnoutrefresh(bottomwin);

    UNSET(CONST_UPDATE);

    ans = mallocstrcpy(NULL, "");

  change_browser_directory:
	/* We go here after we select a new directory. */

    /* Start with no key pressed. */
    kbinput = ERR;

    path = mallocstrassn(path, get_full_path(path));

    /* Save the current path in order to be used later. */
    path_save = path;

    assert(path != NULL && path[strlen(path) - 1] == '/');

    /* Get the file list, and set longest and width in the process. */
    browser_init(path, dir);

    assert(filelist != NULL);

    /* Sort the file list. */
    qsort(filelist, filelist_len, sizeof(char *), diralphasort);

    /* If prev_dir isn't NULL, select the directory saved in it, and
     * then blow it away. */
    if (prev_dir != NULL) {
	browser_select_dirname(prev_dir);

	free(prev_dir);
	prev_dir = NULL;
    /* Otherwise, select the first file or directory in the list. */
    } else
	selected = 0;

    old_selected = (size_t)-1;

    titlebar(path);

    while (TRUE) {
	struct stat st;
	int i;
	size_t fileline = selected / width;
		/* The line number the selected file is on. */
	char *new_path;
		/* The path we switch to at the "Go to Directory"
		 * prompt. */

	/* Make sure that the cursor is off. */
	curs_set(0);

#ifndef NANO_TINY
	if (kbinput == KEY_WINCH) {
	    /* Rebuild the file list and sort it. */
	    browser_init(path_save, opendir(path_save));
	    qsort(filelist, filelist_len, sizeof(char *), diralphasort);

	    /* Make sure the selected file is within range. */
	    if (selected >= filelist_len)
		selected = filelist_len - 1;
	}
#endif
	/* Display (or redisplay) the file list if we don't have a key yet,
	 * or the list has changed, or the selected file has changed. */
	if (kbinput == ERR || kbinput == KEY_WINCH || old_selected != selected)
	    browser_refresh();

	old_selected = selected;

	kbinput = get_kbinput(edit);

#ifndef NANO_TINY
	if (kbinput == KEY_WINCH)
	    continue;
#endif

#ifndef DISABLE_MOUSE
	if (kbinput == KEY_MOUSE) {
	    int mouse_x, mouse_y;

	    /* We can click on the edit window to select a
	     * filename. */
	    if (get_mouseinput(&mouse_x, &mouse_y, TRUE) == 0 &&
		wmouse_trafo(edit, &mouse_y, &mouse_x, FALSE)) {
		/* longest is the width of each column.  There
		 * are two spaces between each column. */
		selected = (fileline / editwinrows) *
				(editwinrows * width) + (mouse_y *
				width) + (mouse_x / (longest + 2));

		/* If they clicked beyond the end of a row,
		 * select the last filename in that row. */
		if (mouse_x > width * (longest + 2))
		    selected--;

		/* If we're off the screen, select the last filename. */
		if (selected > filelist_len - 1)
		    selected = filelist_len - 1;

		/* If we selected the same filename as last time,
		 * put back the Enter key so that it's read in. */
		if (old_selected == selected)
		    unget_kbinput(sc_seq_or(do_enter, 0), FALSE, FALSE);
	    }
	}
#endif /* !DISABLE_MOUSE */

	func = parse_browser_input(&kbinput);

	if (func == total_refresh) {
	    total_redraw();
	} else if (func == do_help_void) {
#ifndef DISABLE_HELP
	    do_help_void();
	    /* The window dimensions might have changed, so act as if. */
	    kbinput = KEY_WINCH;
#else
	    say_there_is_no_help();
#endif
	} else if (func == do_search) {
	    /* Search for a filename. */
	    do_filesearch();
	} else if (func == do_research) {
	    /* Search for another filename. */
	    do_fileresearch();
	} else if (func == do_page_up) {
	    if (selected >= (editwinrows + fileline % editwinrows) * width)
		selected -= (editwinrows + fileline % editwinrows) * width;
	    else
		selected = 0;
	} else if (func == do_page_down) {
	    selected += (editwinrows - fileline % editwinrows) * width;
	    if (selected > filelist_len - 1)
		selected = filelist_len - 1;
	} else if (func == do_first_file) {
	    selected = 0;
	} else if (func == do_last_file) {
	    selected = filelist_len - 1;
	} else if (func == goto_dir_void) {
	    /* Go to a specific directory. */
	    i = do_prompt(TRUE,
#ifndef DISABLE_TABCOMP
			FALSE,
#endif
			MGOTODIR, ans,
#ifndef DISABLE_HISTORIES
			NULL,
#endif
			/* TRANSLATORS: This is a prompt. */
			browser_refresh, _("Go To Directory"));

	    bottombars(MBROWSER);

	    /* If the directory begins with a newline (i.e. an
	     * encoded null), treat it as though it's blank. */
	    if (i < 0 || *answer == '\n') {
		statusbar(_("Cancelled"));
		continue;
	    } else if (i != 0) {
		/* Put back the "Go to Directory" key and save
		 * answer in ans, so that the file list is displayed
		 * again, the prompt is displayed again, and what we
		 * typed before at the prompt is displayed again. */
		unget_kbinput(sc_seq_or(do_gotolinecolumn_void, 0), FALSE, FALSE);
		ans = mallocstrcpy(ans, answer);
		continue;
	    }

	    /* Convert newlines to nulls in the directory name. */
	    sunder(answer);
	    align(&answer);

	    new_path = real_dir_from_tilde(answer);

	    if (new_path[0] != '/') {
		new_path = charealloc(new_path, strlen(path) +
				strlen(answer) + 1);
		sprintf(new_path, "%s%s", path, answer);
	    }

#ifndef DISABLE_OPERATINGDIR
	    if (check_operating_dir(new_path, FALSE)) {
		statusbar(_("Can't go outside of %s in restricted mode"),
				operating_dir);
		free(new_path);
		continue;
	    }
#endif

	    dir = opendir(new_path);
	    if (dir == NULL) {
		/* We can't open this directory for some reason.
		* Complain. */
		statusbar(_("Error reading %s: %s"), answer,
				strerror(errno));
		beep();
		free(new_path);
		continue;
	    }

	    /* Start over again with the new path value. */
	    free(path);
	    path = new_path;
	    goto change_browser_directory;
	} else if (func == do_up_void) {
	    if (selected >= width)
		selected -= width;
	} else if (func == do_down_void) {
	    if (selected + width <= filelist_len - 1)
		selected += width;
	} else if (func == do_left) {
	    if (selected > 0)
		selected--;
	} else if (func == do_right) {
	    if (selected < filelist_len - 1)
		selected++;
	} else if (func == do_enter) {
	    /* We can't move up from "/". */
	    if (strcmp(filelist[selected], "/..") == 0) {
		statusbar(_("Can't move up a directory"));
		beep();
		continue;
	    }

#ifndef DISABLE_OPERATINGDIR
	    /* Note: The selected file can be outside the operating
	     * directory if it's ".." or if it's a symlink to a
	     * directory outside the operating directory. */
	    if (check_operating_dir(filelist[selected], FALSE)) {
		statusbar(_("Can't go outside of %s in restricted mode"),
				operating_dir);
		beep();
		continue;
	    }
#endif

	    if (stat(filelist[selected], &st) == -1) {
		/* We can't open this file for some reason.
		 * Complain. */
		 statusbar(_("Error reading %s: %s"),
				filelist[selected], strerror(errno));
		 beep();
		 continue;
	    }

	    if (!S_ISDIR(st.st_mode)) {
		/* We've successfully opened a file, we're done, so
		 * get out. */
		retval = mallocstrcpy(NULL, filelist[selected]);
		break;
	    }

	    dir = opendir(filelist[selected]);

	    if (dir == NULL) {
		statusbar(_("Error reading %s: %s"),
				filelist[selected], strerror(errno));
		beep();
		continue;
	    }

	    /* If we moved up one level, remember where we came from, so
	     * this directory can be highlighted and easily reentered. */
	    if (strcmp(tail(filelist[selected]), "..") == 0)
		prev_dir = striponedir(filelist[selected]);

	    path = mallocstrcpy(path, filelist[selected]);

	    /* Start over again with the new path value. */
	    goto change_browser_directory;
	} else if (func == do_exit) {
	    /* Exit from the file browser. */
	    break;
	}
    }
    titlebar(NULL);
    edit_refresh();
    if (old_const_update)
	SET(CONST_UPDATE);

    free(path);
    free(ans);

    free_chararray(filelist, filelist_len);
    filelist = NULL;
    filelist_len = 0;

    return retval;
}

/* The file browser front end.  We check to see if inpath has a
 * directory in it.  If it does, we start do_browser() from there.
 * Otherwise, we start do_browser() from the current directory. */
char *do_browse_from(const char *inpath)
{
    struct stat st;
    char *path;
	/* This holds the tilde-expanded version of inpath. */
    DIR *dir = NULL;

    assert(inpath != NULL);

    path = real_dir_from_tilde(inpath);

    /* Perhaps path is a directory.  If so, we'll pass it to
     * do_browser().  Or perhaps path is a directory / a file.  If so,
     * we'll try stripping off the last path element and passing it to
     * do_browser().  Or perhaps path doesn't have a directory portion
     * at all.  If so, we'll just pass the current directory to
     * do_browser(). */
    if (stat(path, &st) == -1 || !S_ISDIR(st.st_mode)) {
	path = mallocstrassn(path, striponedir(path));

	if (stat(path, &st) == -1 || !S_ISDIR(st.st_mode)) {
	    free(path);

	    path = charalloc(PATH_MAX + 1);
	    path = getcwd(path, PATH_MAX + 1);

	    if (path != NULL)
		align(&path);
	}
    }

#ifndef DISABLE_OPERATINGDIR
    /* If the resulting path isn't in the operating directory, use
     * the operating directory instead. */
    if (check_operating_dir(path, FALSE))
	path = mallocstrcpy(path, operating_dir);
#endif

    if (path != NULL)
	dir = opendir(path);

    /* If we can't open the path, get out. */
    if (dir == NULL) {
	free(path);
	beep();
	return NULL;
    }

    return do_browser(path, dir);
}

/* Set filelist to the list of files contained in the directory path,
 * set filelist_len to the number of files in that list, set longest to
 * the width in columns of the longest filename in that list (between 15
 * and COLS), and set width to the number of files that we can display
 * per line.  longest needs to be at least 15 columns in order to
 * display ".. (parent dir)", as Pico does.  Assume path exists and is a
 * directory. */
void browser_init(const char *path, DIR *dir)
{
    const struct dirent *nextdir;
    size_t i = 0, path_len = strlen(path);
    int col = 0;
	/* The maximum number of columns that the filenames will take
	 * up. */
    int line = 0;
	/* The maximum number of lines that the filenames will take
	 * up. */
    int filesperline = 0;
	/* The number of files that we can display per line. */

    assert(path != NULL && path[strlen(path) - 1] == '/' && dir != NULL);

    longest = 0;

    /* Find the length of the longest filename in the current folder. */
    while ((nextdir = readdir(dir)) != NULL) {
	size_t name_len = strlenpt(nextdir->d_name);

	if (name_len > longest)
	    longest = name_len;

	i++;
    }

    /* Put 10 characters' worth of blank space between columns of filenames
     * in the list whenever possible, as Pico does. */
    longest += 10;

    /* Make sure longest is between 15 and COLS. */
    if (longest < 15)
	longest = 15;
    if (longest > COLS)
	longest = COLS;

    rewinddir(dir);

    free_chararray(filelist, filelist_len);

    filelist_len = i;

    filelist = (char **)nmalloc(filelist_len * sizeof(char *));

    i = 0;

    while ((nextdir = readdir(dir)) != NULL && i < filelist_len) {
	/* Don't show the "." entry. */
	if (strcmp(nextdir->d_name, ".") == 0)
	    continue;

	filelist[i] = charalloc(path_len + strlen(nextdir->d_name) + 1);
	sprintf(filelist[i], "%s%s", path, nextdir->d_name);

	i++;
    }

    /* Maybe the number of files in the directory changed between the
     * first time we scanned and the second.  i is the actual length of
     * filelist, so record it. */
    filelist_len = i;

    closedir(dir);

    /* Set width to zero, just before we initialize it. */
    width = 0;

    for (i = 0; i < filelist_len && line < editwinrows; i++) {
	/* Calculate the number of columns one filename will take up. */
	col += longest;
	filesperline++;

	/* Add some space between the columns. */
	col += 2;

	/* If the next entry isn't going to fit on the current line,
	 * move to the next line. */
	if (col > COLS - longest) {
	    line++;
	    col = 0;

	    /* If width isn't initialized yet, and we've taken up more
	     * than one line, it means that width is equal to
	     * filesperline. */
	    if (width == 0)
		width = filesperline;
	}
    }

    /* If width isn't initialized yet, and we've taken up only one line,
     * it means that width is equal to longest. */
    if (width == 0)
	width = longest;
}

/* Return the function that is bound to the given key, accepting certain
 * plain characters too, for compatibility with Pico. */
functionptrtype parse_browser_input(int *kbinput)
{
    if (!meta_key) {
	switch (*kbinput) {
	    case ' ':
		return do_page_down;
	    case '-':
		return do_page_up;
	    case '?':
		return do_help_void;
	    case 'E':
	    case 'e':
		return do_exit;
	    case 'G':
	    case 'g':
		return goto_dir_void;
	    case 'S':
	    case 's':
		return do_enter;
	    case 'W':
	    case 'w':
		return do_search;
	}
    }
    return func_from_key(kbinput);
}

/* Set width to the number of files that we can display per line, if
 * necessary, and display the list of files. */
void browser_refresh(void)
{
    size_t i;
    int line = 0, col = 0;
	/* The current line and column while the list is getting displayed. */
    char *foo;
	/* The additional information that we'll display about a file. */

    titlebar(path_save);
    blank_edit();

    wmove(edit, 0, 0);

    i = width * editwinrows * ((selected / width) / editwinrows);

    for (; i < filelist_len && line < editwinrows; i++) {
	struct stat st;
	const char *filetail = tail(filelist[i]);
		/* The filename we display, minus the path. */
	size_t filetaillen = strlenpt(filetail);
		/* The length of the filename in columns. */
	size_t foolen;
		/* The length of the file information in columns. */
	int foomaxlen = 7;
		/* The maximum length of the file information in
		 * columns: seven for "--", "(dir)", or the file size,
		 * and 12 for "(parent dir)". */
	bool dots = (COLS >= 15 && filetaillen >= longest - foomaxlen);
		/* Do we put an ellipsis before the filename?  Don't set
		 * this to TRUE if we have fewer than 15 columns (i.e.
		 * one column for padding, plus seven columns for a
		 * filename other than ".."). */
	char *disp = display_string(filetail, dots ? filetaillen -
		longest + foomaxlen + 4 : 0, longest, FALSE);
		/* If we put an ellipsis before the filename, reserve
		 * one column for padding, plus seven columns for "--",
		 * "(dir)", or the file size, plus three columns for the
		 * ellipsis. */

	/* Start highlighting the currently selected file or directory. */
	if (i == selected)
	    wattron(edit, hilite_attribute);

	blank_line(edit, line, col, longest);

	/* If dots is TRUE, we will display something like "...ename". */
	if (dots)
	    mvwaddstr(edit, line, col, "...");
	mvwaddstr(edit, line, dots ? col + 3 : col, disp);

	free(disp);

	col += longest;

	/* Show information about the file.  We don't want to report
	 * file sizes for links, so we use lstat(). */
	if (lstat(filelist[i], &st) == -1 || S_ISLNK(st.st_mode)) {
	    /* If the file doesn't exist (i.e. it's been deleted while
	     * the file browser is open), or it's a symlink that doesn't
	     * point to a directory, display "--". */
	    if (stat(filelist[i], &st) == -1 || !S_ISDIR(st.st_mode))
		foo = mallocstrcpy(NULL, "--");
	    /* If the file is a symlink that points to a directory,
	     * display it as a directory. */
	    else
		/* TRANSLATORS: Try to keep this at most 7 characters. */
		foo = mallocstrcpy(NULL, _("(dir)"));
	} else if (S_ISDIR(st.st_mode)) {
	    /* If the file is a directory, display it as such. */
	    if (strcmp(filetail, "..") == 0) {
		/* TRANSLATORS: Try to keep this at most 12 characters. */
		foo = mallocstrcpy(NULL, _("(parent dir)"));
		foomaxlen = 12;
	    } else
		foo = mallocstrcpy(NULL, _("(dir)"));
	} else {
	    off_t result = st.st_size;
	    char modifier;

	    foo = charalloc(foomaxlen + 1);

	    if (st.st_size < (1 << 10))
		modifier = ' ';  /* bytes */
	    else if (st.st_size < (1 << 20)) {
		result >>= 10;
		modifier = 'K';  /* kilobytes */
	    } else if (st.st_size < (1 << 30)) {
		result >>= 20;
		modifier = 'M';  /* megabytes */
	    } else {
		result >>= 30;
		modifier = 'G';  /* gigabytes */
	    }

	    /* Show the size if less than a terabyte,
	     * otherwise show "(huge)". */
	    if (result < (1 << 10))
		sprintf(foo, "%4ju %cB", (intmax_t)result, modifier);
	    else
		/* TRANSLATORS: Try to keep this at most 7 characters.
		 * If necessary, you can leave out the parentheses. */
		foo = mallocstrcpy(foo, _("(huge)"));
	}

	/* Make sure foo takes up no more than foomaxlen columns. */
	foolen = strlenpt(foo);
	if (foolen > foomaxlen) {
	    null_at(&foo, actual_x(foo, foomaxlen));
	    foolen = foomaxlen;
	}

	mvwaddstr(edit, line, col - foolen, foo);

	/* Finish highlighting the currently selected file or directory. */
	if (i == selected)
	    wattroff(edit, hilite_attribute);

	free(foo);

	/* Add some space between the columns. */
	col += 2;

	/* If the next entry isn't going to fit on the current line,
	 * move to the next line. */
	if (col > COLS - longest) {
	    line++;
	    col = 0;
	}

	wmove(edit, line, col);
    }

    wnoutrefresh(edit);
}

/* Look for needle.  If we find it, set selected to its location.
 * Note that needle must be an exact match for a file in the list. */
void browser_select_dirname(const char *needle)
{
    size_t looking_at = 0;

    for (; looking_at < filelist_len; looking_at++) {
	if (strcmp(filelist[looking_at], needle) == 0) {
	    selected = looking_at;
	    break;
	}
    }
}

/* Set up the system variables for a filename search.  Return -1 or -2 if
 * the search should be canceled (due to Cancel or a blank search string),
 * return 0 when we have a string, and return a positive value when some
 * function was run. */
int filesearch_init(void)
{
    int input;
    char *buf;

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
    input = do_prompt(FALSE,
#ifndef DISABLE_TABCOMP
	TRUE,
#endif
	MWHEREISFILE, NULL,
#ifndef DISABLE_HISTORIES
	&search_history,
#endif
	browser_refresh, "%s%s", _("Search"), buf);

    /* Release buf now that we don't need it anymore. */
    free(buf);

    /* If only Enter was pressed but we have a previous string, it's okay. */
    if (input == -2 && *last_search != '\0')
	return 0;

    /* Otherwise negative inputs are a bailout. */
    if (input < 0)
	statusbar(_("Cancelled"));

    return input;
}

/* Look for the given needle in the list of files. */
void findnextfile(const char *needle)
{
    size_t looking_at = selected;
	/* The location in the file list of the filename we're looking at. */
    bool came_full_circle = FALSE;
	/* Have we reached the starting file again? */
    const char *filetail = tail(filelist[looking_at]);
	/* The filename we display, minus the path. */
    const char *rev_start = filetail, *found = NULL;
    unsigned stash[sizeof(flags) / sizeof(flags[0])];
	/* A storage place for the current flag settings. */

    /* Save the settings of all flags. */
    memcpy(stash, flags, sizeof(flags));

    /* Search forward, case insensitive and without regexes. */
    UNSET(BACKWARDS_SEARCH);
    UNSET(CASE_SENSITIVE);
    UNSET(USE_REGEXP);

    /* Step through each filename in the list until a match is found or
     * we've come back to the point where we started. */
    while (TRUE) {
	found = strstrwrapper(filetail, needle, rev_start);

	/* If we've found a match and it's not the same filename where
	 * we started, then we're done. */
	if (found != NULL && looking_at != selected)
	    break;

	/* If we've found a match and we're back at the beginning, then
	 * it's the only occurrence. */
	if (found != NULL && came_full_circle) {
	    statusbar(_("This is the only occurrence"));
	    break;
	}

	if (came_full_circle) {
	    /* We're back at the beginning and didn't find anything. */
	    not_found_msg(needle);
	    return;
	}

	/* Move to the next filename in the list.  If we've reached the
	 * end of the list, wrap around. */
	if (looking_at < filelist_len - 1)
	    looking_at++;
	else {
	    looking_at = 0;
	    statusbar(_("Search Wrapped"));
	}

	if (looking_at == selected)
	    /* We've reached the original starting file. */
	    came_full_circle = TRUE;

	filetail = tail(filelist[looking_at]);

	rev_start = filetail;
    }

    /* Restore the settings of all flags. */
    memcpy(flags, stash, sizeof(flags));

    /* Select the one we've found. */
    selected = looking_at;
}

/* Search for a filename. */
void do_filesearch(void)
{
    if (filesearch_init() != 0) {
	/* Cancelled, or a blank search string, or done something. */
	bottombars(MBROWSER);
	return;
    }

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

    findnextfile(answer);

    bottombars(MBROWSER);
}

/* Search for the last given filename again without prompting. */
void do_fileresearch(void)
{
    if (last_search[0] == '\0')
	statusbar(_("No current search pattern"));
    else
	findnextfile(last_search);
}

/* Select the first file in the list. */
void do_first_file(void)
{
    selected = 0;
}

/* Select the last file in the list. */
void do_last_file(void)
{
    selected = filelist_len - 1;
}

/* Strip one directory from the end of path, and return the stripped
 * path.  The returned string is dynamically allocated, and should be
 * freed. */
char *striponedir(const char *path)
{
    char *retval, *tmp;

    assert(path != NULL);

    retval = mallocstrcpy(NULL, path);

    tmp = strrchr(retval, '/');

    if (tmp != NULL)
	null_at(&retval, tmp - retval);

    return retval;
}

#endif /* !DISABLE_BROWSER */
