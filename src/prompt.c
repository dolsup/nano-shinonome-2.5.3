/* $Id: prompt.c 5670 2016-02-23 08:31:57Z bens $ */
/**************************************************************************
 *   prompt.c                                                             *
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
#include <string.h>

static char *prompt = NULL;
	/* The prompt string used for statusbar questions. */
static size_t statusbar_x = (size_t)-1;
	/* The cursor position in answer. */
static size_t statusbar_pww = (size_t)-1;
	/* The place we want in answer. */
static size_t old_statusbar_x = (size_t)-1;
	/* The old cursor position in answer, if any. */
static size_t old_pww = (size_t)-1;
	/* The old place we want in answer, if any. */

/* Read in a character, interpret it as a shortcut or toggle if
 * necessary, and return it.
 * Set ran_func to TRUE if we ran a function associated with a
 * shortcut key, and set finished to TRUE if we're done after running
 * or trying to run a function associated with a shortcut key.
 * refresh_func is the function we will call to refresh the edit window. */
int do_statusbar_input(bool *ran_func, bool *finished,
	void (*refresh_func)(void))
{
    int input;
	/* The character we read in. */
    static int *kbinput = NULL;
	/* The input buffer. */
    static size_t kbinput_len = 0;
	/* The length of the input buffer. */
    const sc *s;
    bool have_shortcut = FALSE;
    const subnfunc *f;

    *ran_func = FALSE;
    *finished = FALSE;

    /* Read in a character. */
    input = get_kbinput(bottomwin);

#ifndef NANO_TINY
    if (input == KEY_WINCH)
	return KEY_WINCH;
#endif

#ifndef DISABLE_MOUSE
    /* If we got a mouse click and it was on a shortcut, read in the
     * shortcut character. */
    if (func_key && input == KEY_MOUSE) {
	if (do_statusbar_mouse() == 1)
	    input = get_kbinput(bottomwin);
	else {
	    meta_key = FALSE;
	    func_key = FALSE;
	    input = ERR;
	}
    }
#endif

    /* Check for a shortcut in the current list. */
    s = get_shortcut(&input);

    /* If we got a shortcut from the current list, or a "universal"
     * statusbar prompt shortcut, set have_shortcut to TRUE. */
    have_shortcut = (s != NULL);

    /* If we got a non-high-bit control key, a meta key sequence, or a
     * function key, and it's not a shortcut or toggle, throw it out. */
    if (!have_shortcut) {
	if (is_ascii_cntrl_char(input) || meta_key || func_key) {
	    beep();
	    meta_key = FALSE;
	    func_key = FALSE;
	    input = ERR;
	}
    }

    /* If we got a character, and it isn't a shortcut or toggle,
     * it's a normal text character.  Display the warning if we're
     * in view mode, or add the character to the input buffer if
     * we're not. */
    if (input != ERR && !have_shortcut) {
	/* If we're using restricted mode, the filename isn't blank,
	 * and we're at the "Write File" prompt, disable text input. */
	if (!ISSET(RESTRICTED) || openfile->filename[0] == '\0' ||
		currmenu != MWRITEFILE) {
	    kbinput_len++;
	    kbinput = (int *)nrealloc(kbinput, kbinput_len * sizeof(int));
	    kbinput[kbinput_len - 1] = input;
	}
     }

    /* If we got a shortcut, or if there aren't any other characters
     * waiting after the one we read in, we need to display all the
     * characters in the input buffer if it isn't empty. */
    if (have_shortcut || get_key_buffer_len() == 0) {
	if (kbinput != NULL) {
	    /* Display all the characters in the input buffer at
	     * once, filtering out control characters. */
	    do_statusbar_output(kbinput, kbinput_len, TRUE, NULL);

	    /* Empty the input buffer. */
	    kbinput_len = 0;
	    free(kbinput);
	    kbinput = NULL;
	}

	if (have_shortcut) {
	    if (s->scfunc == do_tab || s->scfunc == do_enter)
		;
	    else if (s->scfunc == total_refresh) {
		total_redraw();
		refresh_func();
	    } else if (s->scfunc == do_cut_text_void) {
		/* If we're using restricted mode, the filename
		 * isn't blank, and we're at the "Write File"
		 * prompt, disable Cut. */
		if (!ISSET(RESTRICTED) || openfile->filename[0] == '\0' ||
			currmenu != MWRITEFILE)
		    do_statusbar_cut_text();
	    } else if (s->scfunc == do_left)
		do_statusbar_left();
	    else if (s->scfunc == do_right)
		do_statusbar_right();
#ifndef NANO_TINY
	    else if (s->scfunc == do_prev_word_void)
		do_statusbar_prev_word();
	    else if (s->scfunc == do_next_word_void)
		do_statusbar_next_word();
#endif
	    else if (s->scfunc == do_home)
		do_statusbar_home();
	    else if (s->scfunc == do_end)
		do_statusbar_end();
	    else if (s->scfunc == do_verbatim_input) {
		/* If we're using restricted mode, the filename
		 * isn't blank, and we're at the "Write File"
		 * prompt, disable verbatim input. */
		if (!ISSET(RESTRICTED) || currmenu != MWRITEFILE ||
			openfile->filename[0] == '\0') {
		    bool got_enter = FALSE;
			/* Whether we got the Enter key. */

		    do_statusbar_verbatim_input(&got_enter);

		    /* If we got the Enter key, remove it from the input
		     * buffer, set input to the key value for Enter, and
		     * set finished to TRUE to indicate that we're done. */
		    if (got_enter) {
			get_input(NULL, 1);
			input = sc_seq_or(do_enter, 0);
			*finished = TRUE;
		    }
		}
	    } else if (s->scfunc == do_delete) {
		/* If we're using restricted mode, the filename
		 * isn't blank, and we're at the "Write File"
		 * prompt, disable Delete. */
		if (!ISSET(RESTRICTED) || openfile->filename[0] == '\0' ||
			currmenu != MWRITEFILE)
		    do_statusbar_delete();
	    } else if (s->scfunc == do_backspace) {
		/* If we're using restricted mode, the filename
		 * isn't blank, and we're at the "Write File"
		 * prompt, disable Backspace. */
		if (!ISSET(RESTRICTED) || openfile->filename[0] == '\0' ||
			currmenu != MWRITEFILE)
		    do_statusbar_backspace();
	    } else {
		/* Handle any other shortcut in the current menu, setting
		 * ran_func to TRUE if we try to run their associated
		 * functions and setting finished to TRUE to indicate
		 * that we're done after running or trying to run their
		 * associated functions. */
		f = sctofunc(s);
		if (s->scfunc != NULL) {
		    *ran_func = TRUE;
		    if (f && (!ISSET(VIEW_MODE) || f->viewok) &&
				f->scfunc != do_gotolinecolumn_void)
			f->scfunc();
		}
		*finished = TRUE;
	    }
	}
    }

    return input;
}

#ifndef DISABLE_MOUSE
/* Handle a mouse click on the statusbar prompt or the shortcut list. */
int do_statusbar_mouse(void)
{
    int mouse_x, mouse_y;
    int retval = get_mouseinput(&mouse_x, &mouse_y, TRUE);

    /* We can click on the statusbar window text to move the cursor. */
    if (retval == 0 && wmouse_trafo(bottomwin, &mouse_y, &mouse_x, FALSE)) {
	size_t start_col;

	assert(prompt != NULL);

	start_col = strlenpt(prompt) + 2;

	/* Move to where the click occurred. */
	if (mouse_x >= start_col && mouse_y == 0) {
	    statusbar_x = actual_x(answer,
			get_statusbar_page_start(start_col, start_col +
			statusbar_xplustabs()) + mouse_x - start_col);
	    update_bar_if_needed();
	}
    }

    return retval;
}
#endif

/* The user typed input_len multibyte characters.  Add them to the
 * statusbar prompt, setting got_enter to TRUE if we get a newline,
 * and filtering out ASCII control characters if filtering is TRUE. */
void do_statusbar_output(int *the_input, size_t input_len,
	bool filtering, bool *got_enter)
{
    char *output = charalloc(input_len + 1);
    char *char_buf = charalloc(mb_cur_max());
    int i, char_len;

    assert(answer != NULL);

    /* Copy the typed stuff so it can be treated. */
    for (i = 0; i < input_len; i++)
	output[i] = (char)the_input[i];
    output[i] = '\0';

    i = 0;

    while (i < input_len) {
	/* When not filtering, convert nulls and stop at a newline. */
	if (!filtering) {
	    if (output[i] == '\0')
		output[i] = '\n';
	    else if (output[i] == '\n') {
		/* Put back the rest of the characters for reparsing,
		 * indicate that we got the Enter key and get out. */
		unparse_kbinput(output + i, input_len - i);
		*got_enter = TRUE;
		return;
	    }
	}

	/* Interpret the next multibyte character. */
	char_len = parse_mbchar(output + i, char_buf, NULL);

	i += char_len;

	/* When filtering, skip any ASCII control character. */
	if (filtering && is_ascii_cntrl_char(*(output + i - char_len)))
	    continue;

	assert(statusbar_x <= strlen(answer));

	/* Insert the typed character into the existing answer string. */
	answer = charealloc(answer, strlen(answer) + char_len + 1);
	charmove(answer + statusbar_x + char_len, answer + statusbar_x,
				strlen(answer) - statusbar_x + 1);
	strncpy(answer + statusbar_x, char_buf, char_len);

	statusbar_x += char_len;
    }

    free(char_buf);
    free(output);

    statusbar_pww = statusbar_xplustabs();

    update_the_statusbar();
}

/* Move to the beginning of the prompt text. */
void do_statusbar_home(void)
{
    statusbar_x = 0;
    update_bar_if_needed();
}

/* Move to the end of the prompt text. */
void do_statusbar_end(void)
{
    statusbar_x = strlen(answer);
    update_bar_if_needed();
}

/* Move left one character. */
void do_statusbar_left(void)
{
    if (statusbar_x > 0) {
	statusbar_x = move_mbleft(answer, statusbar_x);
	update_bar_if_needed();
    }
}

/* Move right one character. */
void do_statusbar_right(void)
{
    if (statusbar_x < strlen(answer)) {
	statusbar_x = move_mbright(answer, statusbar_x);
	update_bar_if_needed();
    }
}

/* Backspace over one character. */
void do_statusbar_backspace(void)
{
    if (statusbar_x > 0) {
	statusbar_x = move_mbleft(answer, statusbar_x);
	do_statusbar_delete();
    }
}

/* Delete one character. */
void do_statusbar_delete(void)
{
    statusbar_pww = statusbar_xplustabs();

    if (answer[statusbar_x] != '\0') {
	int char_len = parse_mbchar(answer + statusbar_x, NULL, NULL);

	assert(statusbar_x < strlen(answer));

	charmove(answer + statusbar_x, answer + statusbar_x + char_len,
			strlen(answer) - statusbar_x - char_len + 1);
	align(&answer);

	update_the_statusbar();
    }
}

/* Move text from the prompt into oblivion. */
void do_statusbar_cut_text(void)
{
    assert(answer != NULL);

#ifndef NANO_TINY
    if (ISSET(CUT_TO_END))
	null_at(&answer, statusbar_x);
    else
#endif
    {
	null_at(&answer, 0);
	statusbar_x = 0;
	statusbar_pww = statusbar_xplustabs();
    }

    update_the_statusbar();
}

#ifndef NANO_TINY
/* Move to the next word in the prompt text. */
void do_statusbar_next_word(void)
{
    bool seen_space = !is_word_mbchar(answer + statusbar_x, FALSE);

    assert(answer != NULL);

    /* Move forward until we reach the start of a word. */
    while (answer[statusbar_x] != '\0') {
	statusbar_x = move_mbright(answer, statusbar_x);

	/* If this is not a word character, then it's a separator; else
	 * if we've already seen a separator, then it's a word start. */
	if (!is_word_mbchar(answer + statusbar_x, FALSE))
	    seen_space = TRUE;
	else if (seen_space)
	    break;
    }

    update_bar_if_needed();
}

/* Move to the previous word in the prompt text. */
void do_statusbar_prev_word(void)
{
    bool seen_a_word = FALSE, step_forward = FALSE;

    assert(answer != NULL);

    /* Move backward until we pass over the start of a word. */
    while (statusbar_x != 0) {
	statusbar_x = move_mbleft(answer, statusbar_x);

	if (is_word_mbchar(answer + statusbar_x, FALSE))
	    seen_a_word = TRUE;
	else if (seen_a_word) {
	    /* This is space now: we've overshot the start of the word. */
	    step_forward = TRUE;
	    break;
	}
    }

    if (step_forward)
	/* Move one character forward again to sit on the start of the word. */
	statusbar_x = move_mbright(answer, statusbar_x);

    update_bar_if_needed();
}
#endif /* !NANO_TINY */

/* Get verbatim input.  Set got_enter to TRUE if we got the Enter key as
 * part of the verbatim input. */
void do_statusbar_verbatim_input(bool *got_enter)
{
    int *kbinput;
    size_t kbinput_len;

    /* Read in all the verbatim characters. */
    kbinput = get_verbatim_kbinput(bottomwin, &kbinput_len);

    /* Display all the verbatim characters at once, not filtering out
     * control characters. */
    do_statusbar_output(kbinput, kbinput_len, FALSE, got_enter);
}

/* Return the placewewant associated with statusbar_x, i.e. the
 * zero-based column position of the cursor.  The value will be no
 * smaller than statusbar_x. */
size_t statusbar_xplustabs(void)
{
    return strnlenpt(answer, statusbar_x);
}

/* nano scrolls horizontally within a line in chunks.  This function
 * returns the column number of the first character displayed in the
 * statusbar prompt when the cursor is at the given column with the
 * prompt ending at start_col.  Note that (0 <= column -
 * get_statusbar_page_start(column) < COLS). */
size_t get_statusbar_page_start(size_t start_col, size_t column)
{
    if (column == start_col || column < COLS - 1 || COLS == start_col + 1)
	return 0;
    else
	return column - start_col - (column - start_col) % (COLS -
		start_col - 1);
}

/* Put the cursor in the statusbar prompt at statusbar_x. */
void reset_statusbar_cursor(void)
{
    size_t start_col = strlenpt(prompt) + 2;
    size_t xpt = statusbar_xplustabs();

    wmove(bottomwin, 0, start_col + xpt -
	get_statusbar_page_start(start_col, start_col + xpt));
}

/* Repaint the statusbar. */
void update_the_statusbar(void)
{
    size_t start_col, index, page_start;
    char *expanded;

    assert(prompt != NULL && statusbar_x <= strlen(answer));

    start_col = strlenpt(prompt) + 2;
    index = strnlenpt(answer, statusbar_x);
    page_start = get_statusbar_page_start(start_col, start_col + index);

    if (interface_color_pair[TITLE_BAR].bright)
	wattron(bottomwin, A_BOLD);
    wattron(bottomwin, interface_color_pair[TITLE_BAR].pairnum);

    blank_statusbar();

    mvwaddnstr(bottomwin, 0, 0, prompt, actual_x(prompt, COLS - 2));
    waddch(bottomwin, ':');
    waddch(bottomwin, (page_start == 0) ? ' ' : '$');

    expanded = display_string(answer, page_start, COLS - start_col - 1, FALSE);
    waddstr(bottomwin, expanded);
    free(expanded);

    wattroff(bottomwin, A_BOLD);
    wattroff(bottomwin, interface_color_pair[TITLE_BAR].pairnum);

    statusbar_pww = statusbar_xplustabs();
    reset_statusbar_cursor();
    wnoutrefresh(bottomwin);
}

/* Update the statusbar line /if/ the placewewant changes page. */
void update_bar_if_needed(void)
{
    size_t start_col = strlenpt(prompt) + 2;
    size_t was_pww = statusbar_pww;

    statusbar_pww = statusbar_xplustabs();

    if (get_statusbar_page_start(start_col, start_col + statusbar_pww) !=
		get_statusbar_page_start(start_col, start_col + was_pww))
	update_the_statusbar();
}

/* Get a string of input at the statusbar prompt. */
functionptrtype get_prompt_string(int *actual, bool allow_tabs,
#ifndef DISABLE_TABCOMP
	bool allow_files,
	bool *list,
#endif
	const char *curranswer,
#ifndef DISABLE_HISTORIES
	filestruct **history_list,
#endif
	void (*refresh_func)(void))
{
    int kbinput = ERR;
    bool ran_func, finished;
    functionptrtype func;
#ifndef DISABLE_TABCOMP
    bool tabbed = FALSE;
	/* Whether we've pressed Tab. */
#endif
#ifndef DISABLE_HISTORIES
    char *history = NULL;
	/* The current history string. */
    char *magichistory = NULL;
	/* The temporary string typed at the bottom of the history, if
	 * any. */
#ifndef DISABLE_TABCOMP
    int last_kbinput = ERR;
	/* The key we pressed before the current key. */
    size_t complete_len = 0;
	/* The length of the original string that we're trying to
	 * tab complete, if any. */
#endif
#endif /* !DISABLE_HISTORIES */

    answer = mallocstrcpy(answer, curranswer);

    if (statusbar_x > strlen(answer)) {
	statusbar_x = strlen(answer);
	statusbar_pww = statusbar_xplustabs();
    }

#ifdef DEBUG
    fprintf(stderr, "get_prompt_string: answer = \"%s\", statusbar_x = %lu\n", answer, (unsigned long) statusbar_x);
#endif

    update_the_statusbar();

    /* Refresh edit window and statusbar before getting input. */
    wnoutrefresh(edit);
    wnoutrefresh(bottomwin);

    /* If we're using restricted mode, we aren't allowed to change the
     * name of the current file once it has one, because that would
     * allow writing to files not specified on the command line.  In
     * this case, disable all keys that would change the text if the
     * filename isn't blank and we're at the "Write File" prompt. */
    while (TRUE) {
	/* Ensure the cursor is on when waiting for input. */
	curs_set(1);

	kbinput = do_statusbar_input(&ran_func, &finished, refresh_func);
	assert(statusbar_x <= strlen(answer));

#ifndef NANO_TINY
	if (kbinput == KEY_WINCH) {
	    refresh_func();
	    update_the_statusbar();
	    continue;
	}
#endif
	func = func_from_key(&kbinput);

	if (func == do_cancel || func == do_enter)
	    break;

#ifndef DISABLE_TABCOMP
	if (func != do_tab)
	    tabbed = FALSE;

	if (func == do_tab) {
#ifndef DISABLE_HISTORIES
	    if (history_list != NULL) {
		if (last_kbinput != sc_seq_or(do_tab, NANO_CONTROL_I))
		    complete_len = strlen(answer);

		if (complete_len > 0) {
		    answer = get_history_completion(history_list,
					answer, complete_len);
		    statusbar_x = strlen(answer);
		}
	    } else
#endif
	    if (allow_tabs)
		answer = input_tab(answer, allow_files, &statusbar_x,
				   &tabbed, refresh_func, list);

	    update_the_statusbar();
	} else
#endif /* !DISABLE_TABCOMP */
#ifndef DISABLE_HISTORIES
	if (func == get_history_older_void) {
	    if (history_list != NULL) {
		/* If we're scrolling up at the bottom of the history list
		 * and answer isn't blank, save answer in magichistory. */
		if ((*history_list)->next == NULL && answer[0] != '\0')
		    magichistory = mallocstrcpy(magichistory, answer);

		/* Get the older search from the history list and save it in
		 * answer.  If there is no older search, don't do anything. */
		if ((history = get_history_older(history_list)) != NULL) {
		    answer = mallocstrcpy(answer, history);
		    statusbar_x = strlen(answer);
		}

		update_the_statusbar();

		/* This key has a shortcut-list entry when it's used to
		 * move to an older search, which means that finished has
		 * been set to TRUE.  Set it back to FALSE here, so that
		 * we aren't kicked out of the statusbar prompt. */
		finished = FALSE;
	    }
	} else if (func == get_history_newer_void) {
	    if (history_list != NULL) {
		/* Get the newer search from the history list and save it in
		 * answer.  If there is no newer search, don't do anything. */
		if ((history = get_history_newer(history_list)) != NULL) {
		    answer = mallocstrcpy(answer, history);
		    statusbar_x = strlen(answer);
		}

		/* If, after scrolling down, we're at the bottom of the
		 * history list, answer is blank, and magichistory is set,
		 * save magichistory in answer. */
		if ((*history_list)->next == NULL &&
			*answer == '\0' && magichistory != NULL) {
		    answer = mallocstrcpy(answer, magichistory);
		    statusbar_x = strlen(answer);
		}

		update_the_statusbar();

		/* This key has a shortcut-list entry when it's used to
		 * move to a newer search, which means that finished has
		 * been set to TRUE.  Set it back to FALSE here, so that
		 * we aren't kicked out of the statusbar prompt. */
		finished = FALSE;
	    }
	} else
#endif /* !DISABLE_HISTORIES */
	if (func == do_help_void) {
	    update_the_statusbar();

	    /* This key has a shortcut-list entry when it's used to go to
	     * the help browser or display a message indicating that help
	     * is disabled, which means that finished has been set to TRUE.
	     * Set it back to FALSE here, so that we aren't kicked out of
	     * the statusbar prompt. */
	    finished = FALSE;
	}

	/* If we have a shortcut with an associated function, break out if
	 * we're finished after running or trying to run the function. */
	if (finished)
	    break;

#if !defined(DISABLE_HISTORIES) && !defined(DISABLE_TABCOMP)
	last_kbinput = kbinput;
#endif
	reset_statusbar_cursor();
	wnoutrefresh(bottomwin);
    }

#ifndef DISABLE_HISTORIES
    /* Set the current position in the history list to the bottom. */
    if (history_list != NULL) {
	history_reset(*history_list);
	free(magichistory);
    }
#endif

    /* If we're done with this prompt, restore the cursor position
     * to what it was at the /previous/ prompt, in case there was. */
    if (func == do_cancel || func == do_enter) {
	statusbar_x = old_statusbar_x;
	statusbar_pww = old_pww;
    }

    *actual = kbinput;

    return func;
}

/* Ask a question on the statusbar.  The prompt will be stored in the
 * static prompt, which should be NULL initially, and the answer will be
 * stored in the answer global.  Returns -1 on aborted enter, -2 on a
 * blank string, and 0 otherwise, the valid shortcut key caught.
 * curranswer is any editable text that we want to put up by default,
 * and refresh_func is the function we want to call to refresh the edit
 * window.
 *
 * The allow_tabs parameter indicates whether we should allow tabs to be
 * interpreted.  The allow_files parameter indicates whether we should
 * allow all files (as opposed to just directories) to be tab completed. */
int do_prompt(bool allow_tabs,
#ifndef DISABLE_TABCOMP
	bool allow_files,
#endif
	int menu, const char *curranswer,
#ifndef DISABLE_HISTORIES
	filestruct **history_list,
#endif
	void (*refresh_func)(void), const char *msg, ...)
{
    va_list ap;
    int retval;
    functionptrtype func;
#ifndef DISABLE_TABCOMP
    bool list = FALSE;
#endif

    prompt = charalloc(((COLS - 4) * mb_cur_max()) + 1);

    bottombars(menu);

    va_start(ap, msg);
    vsnprintf(prompt, (COLS - 4) * mb_cur_max(), msg, ap);
    va_end(ap);
    null_at(&prompt, actual_x(prompt, COLS - 4));

    func = get_prompt_string(&retval, allow_tabs,
#ifndef DISABLE_TABCOMP
	allow_files,
	&list,
#endif
	curranswer,
#ifndef DISABLE_HISTORIES
	history_list,
#endif
	refresh_func);

    free(prompt);
    prompt = NULL;

    /* We're done with the prompt, so save the statusbar cursor
     * position. */
    old_statusbar_x = statusbar_x;
    old_pww = statusbar_pww;

    /* If we left the prompt via Cancel or Enter, set the return value
     * properly. */
    if (func == do_cancel)
	retval = -1;
    else if (func == do_enter)
	retval = (*answer == '\0') ? -2 : 0;

    blank_statusbar();
    wnoutrefresh(bottomwin);

#ifdef DEBUG
    fprintf(stderr, "answer = \"%s\"\n", answer);
#endif

#ifndef DISABLE_TABCOMP
    /* If we've done tab completion, there might be a list of filename
     * matches on the edit window at this point.  Make sure that they're
     * cleared off. */
    if (list)
	refresh_func();
#endif

    return retval;
}

/* Ask a simple Yes/No (and optionally All) question, specified in msg,
 * on the statusbar.  Return 1 for Yes, 0 for No, 2 for All (if all is
 * TRUE when passed in), and -1 for Cancel. */
int do_yesno_prompt(bool all, const char *msg)
{
    int response = -2, width = 16;
    const char *yesstr;		/* String of Yes characters accepted. */
    const char *nostr;		/* Same for No. */
    const char *allstr;		/* And All, surprise! */
    int oldmenu = currmenu;

    assert(msg != NULL);

    /* yesstr, nostr, and allstr are strings of any length.  Each string
     * consists of all single-byte characters accepted as valid
     * characters for that value.  The first value will be the one
     * displayed in the shortcuts. */
    /* TRANSLATORS: For the next three strings, if possible, specify
     * the single-byte shortcuts for both your language and English.
     * For example, in French: "OoYy" for "Oui". */
    yesstr = _("Yy");
    nostr = _("Nn");
    allstr = _("Aa");

    do {
	int kbinput;
	functionptrtype func;
#ifndef DISABLE_MOUSE
	int mouse_x, mouse_y;
#endif

	if (!ISSET(NO_HELP)) {
	    char shortstr[3];
		/* Temporary string for (translated) " Y", " N" and " A". */

	    if (COLS < 32)
		width = COLS / 2;

	    /* Clear the shortcut list from the bottom of the screen. */
	    blank_bottombars();

	    /* Now show the ones for "Yes", "No", "Cancel" and maybe "All". */
	    sprintf(shortstr, " %c", yesstr[0]);
	    wmove(bottomwin, 1, 0);
	    onekey(shortstr, _("Yes"), width);

	    if (all) {
		shortstr[1] = allstr[0];
		wmove(bottomwin, 1, width);
		onekey(shortstr, _("All"), width);
	    }

	    shortstr[1] = nostr[0];
	    wmove(bottomwin, 2, 0);
	    onekey(shortstr, _("No"), width);

	    wmove(bottomwin, 2, width);
	    onekey("^C", _("Cancel"), width);
	}

	if (interface_color_pair[TITLE_BAR].bright)
	    wattron(bottomwin, A_BOLD);
	wattron(bottomwin, interface_color_pair[TITLE_BAR].pairnum);

	blank_statusbar();
	mvwaddnstr(bottomwin, 0, 0, msg, actual_x(msg, COLS - 1));

	wattroff(bottomwin, A_BOLD);
	wattroff(bottomwin, interface_color_pair[TITLE_BAR].pairnum);

	/* Refresh edit window and statusbar before getting input. */
	wnoutrefresh(edit);
	wnoutrefresh(bottomwin);

	currmenu = MYESNO;
	kbinput = get_kbinput(bottomwin);

#ifndef NANO_TINY
	if (kbinput == KEY_WINCH)
	    continue;
#endif

	func = func_from_key(&kbinput);

	if (func == do_cancel)
	    response = -1;
#ifndef DISABLE_MOUSE
	else if (kbinput == KEY_MOUSE) {
	    /* We can click on the Yes/No/All shortcuts to select an answer. */
	    if (get_mouseinput(&mouse_x, &mouse_y, FALSE) == 0 &&
			wmouse_trafo(bottomwin, &mouse_y, &mouse_x, FALSE) &&
			mouse_x < (width * 2) && mouse_y > 0) {
		int x = mouse_x / width;
			/* The x-coordinate among the Yes/No/All shortcuts. */
		int y = mouse_y - 1;
			/* The y-coordinate among the Yes/No/All shortcuts. */

		assert(0 <= x && x <= 1 && 0 <= y && y <= 1);

		/* x == 0 means they clicked Yes or No.
		 * y == 0 means Yes or All. */
		response = -2 * x * y + x - y + 1;

		if (response == 2 && !all)
		    response = -2;
	    }
	}
#endif /* !DISABLE_MOUSE */
	else if (func == total_refresh) {
	    total_redraw();
	    continue;
	} else {
	    /* Look for the kbinput in the Yes, No (and All) strings. */
	    if (strchr(yesstr, kbinput) != NULL)
		response = 1;
	    else if (strchr(nostr, kbinput) != NULL)
		response = 0;
	    else if (all && strchr(allstr, kbinput) != NULL)
		response = 2;
	}
    } while (response == -2);

    currmenu = oldmenu;
    return response;
}
