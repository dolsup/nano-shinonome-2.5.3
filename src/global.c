/* $Id: global.c 5659 2016-02-22 09:34:33Z bens $ */
/**************************************************************************
 *   global.c                                                             *
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

#include <ctype.h>
#include <string.h>
#include <strings.h>
#include "assert.h"

/* Global variables. */
#ifndef NANO_TINY
volatile sig_atomic_t sigwinch_counter = 0;
	/* Is incremented by the handler whenever a SIGWINCH occurs. */
#endif

bool meta_key;
	/* Whether the current keystroke is a Meta key. */
bool func_key;
	/* Whether the current keystroke is an extended keypad value. */
bool focusing = FALSE;
	/* Whether an update of the edit window should center the cursor. */

#ifndef NANO_TINY
int controlleft = CONTROL_LEFT;
int controlright = CONTROL_RIGHT;
#endif

#ifndef DISABLE_WRAPJUSTIFY
ssize_t fill = 0;
	/* The column where we will wrap lines. */
ssize_t wrap_at = -CHARS_FROM_EOL;
	/* The position where we will wrap lines.  fill is equal to this
	 * if it's greater than zero, and equal to (COLS + this) if it
	 * isn't. */
#endif

char *last_search = NULL;
	/* The last string we searched for. */
char *last_replace = NULL;
	/* The last replacement string we searched for. */

unsigned flags[4] = {0, 0, 0, 0};
	/* Our flag containing the states of all global options. */
WINDOW *topwin;
	/* The top portion of the window, where we display the version
	 * number of nano, the name of the current file, and whether the
	 * current file has been modified. */
WINDOW *edit;
	/* The middle portion of the window, i.e. the edit window, where
	 * we display the current file we're editing. */
WINDOW *bottomwin;
	/* The bottom portion of the window, where we display statusbar
	 * messages, the statusbar prompt, and a list of shortcuts. */
int editwinrows = 0;
	/* How many rows does the edit window take up? */
int maxrows = 0;
	/* How many usable lines there are (due to soft wrapping). */

filestruct *cutbuffer = NULL;
	/* The buffer where we store cut text. */
filestruct *cutbottom = NULL;
#ifndef DISABLE_JUSTIFY
filestruct *jusbuffer = NULL;
	/* The buffer where we store unjustified text. */
#endif
partition *filepart = NULL;
	/* The partition where we store a portion of the current
	 * file. */
openfilestruct *openfile = NULL;
	/* The list of all open file buffers. */

#ifndef NANO_TINY
char *matchbrackets = NULL;
	/* The opening and closing brackets that can be found by bracket
	 * searches. */
char *whitespace = NULL;
	/* The characters used when visibly showing tabs and spaces. */
int whitespace_len[2];
	/* The length in bytes of these characters. */
#endif

#ifndef DISABLE_JUSTIFY
char *punct = NULL;
	/* The closing punctuation that can end sentences. */
char *brackets = NULL;
	/* The closing brackets that can follow closing punctuation and
	 * can end sentences. */
char *quotestr = NULL;
	/* The quoting string.  The default value is set in main(). */
#ifdef HAVE_REGEX_H
regex_t quotereg;
	/* The compiled regular expression from the quoting string. */
int quoterc;
	/* Whether it was compiled successfully. */
char *quoteerr = NULL;
	/* The error message, if it didn't. */
#else
size_t quotelen;
	/* The length of the quoting string in bytes. */
#endif
#endif

bool nodelay_mode = FALSE;
	/* Are we checking for a cancel wile doing something? */

char *answer = NULL;
	/* The answer string used by the statusbar prompt. */

ssize_t tabsize = -1;
	/* The width of a tab in spaces.  The default value is set in
	 * main(). */

#ifndef NANO_TINY
char *backup_dir = NULL;
	/* The directory where we store backup files. */

const char *locking_prefix = ".";
	/* Prefix of how to store the vim-style lock file. */
const char *locking_suffix = ".swp";
	/* Suffix of the vim-style lock file. */
#endif
#ifndef DISABLE_OPERATINGDIR
char *operating_dir = NULL;
	/* The relative path to the operating directory, which we can't
	 * move outside of. */
char *full_operating_dir = NULL;
	/* The full path to it. */
#endif

#ifndef DISABLE_SPELLER
char *alt_speller = NULL;
	/* The command to use for the alternate spell checker. */
#endif

#ifndef DISABLE_COLOR
syntaxtype *syntaxes = NULL;
	/* The global list of color syntaxes. */
char *syntaxstr = NULL;
	/* The color syntax name specified on the command line. */
#endif

bool edit_refresh_needed = FALSE;
	/* Did a command mangle enough of the buffer refresh that we
	 * should repaint the screen? */

int currmenu;
	/* The currently loaded menu. */
sc *sclist = NULL;
	/* Pointer to the start of the shortcuts list. */
subnfunc *allfuncs = NULL;
	/* Pointer to the start of the functions list. */
subnfunc *tailfunc;
	/* Pointer to the last function in the list. */
subnfunc *exitfunc;
	/* Pointer to the special Exit/Close item. */
subnfunc *uncutfunc;
	/* Pointer to the special Uncut/Unjustify item. */

#ifndef DISABLE_HISTORIES
filestruct *search_history = NULL;
	/* The search string history list. */
filestruct *searchage = NULL;
	/* The top of the search string history list. */
filestruct *searchbot = NULL;
	/* The bottom of the search string history list. */
filestruct *replace_history = NULL;
	/* The replace string history list. */
filestruct *replaceage = NULL;
	/* The top of the replace string history list. */
filestruct *replacebot = NULL;
	/* The bottom of the replace string history list. */
poshiststruct *position_history = NULL;
	/* The cursor position history list. */
#endif

/* Regular expressions. */
#ifdef HAVE_REGEX_H
regex_t search_regexp;
	/* The compiled regular expression to use in searches. */
regmatch_t regmatches[10];
	/* The match positions for parenthetical subexpressions, 10
	 * maximum, used in regular expression searches. */
#endif

int hilite_attribute = A_REVERSE;
	/* The curses attribute we use for reverse video. */
#ifndef DISABLE_COLOR
char* specified_color_combo[] = {};
	/* The color combinations as specified in the rcfile. */
#endif
color_pair interface_color_pair[] = {};
	/* The processed color pairs for the interface elements. */

char *homedir = NULL;
	/* The user's home directory, from $HOME or /etc/passwd. */

/* Return the number of entries in the shortcut list for a given menu. */
size_t length_of_list(int menu)
{
    subnfunc *f;
    size_t i = 0;

    for (f = allfuncs; f != NULL; f = f->next)
	if ((f->menus & menu) != 0) {
	    i++;
	}
    return i;
}

/* To make the functions and shortcuts lists clearer. */
#define VIEW  TRUE		/* Is allowed in view mode. */
#define NOVIEW  FALSE
#define BLANKAFTER  TRUE	/* A blank line after this one. */
#define TOGETHER  FALSE

/* Just throw this here. */
void case_sens_void(void)
{
}
void regexp_void(void)
{
}
void backwards_void(void)
{
}
void gototext_void(void)
{
}
#ifndef DISABLE_BROWSER
void to_files_void(void)
{
}
void goto_dir_void(void)
{
}
#endif
void dos_format_void(void)
{
}
void mac_format_void(void)
{
}
void append_void(void)
{
}
void prepend_void(void)
{
}
void backup_file_void(void)
{
}
void discard_buffer(void)
{
}
void new_buffer_void(void)
{
}
void flip_replace_void(void)
{
}
void flip_execute_void(void)
{
}

/* Add a function to the function list. */
void add_to_funcs(void (*func)(void), int menus, const char *desc, const char *help,
    bool blank_after, bool viewok)
{
    subnfunc *f = (subnfunc *)nmalloc(sizeof(subnfunc));

    if (allfuncs == NULL)
	allfuncs = f;
    else
	tailfunc->next = f;
    tailfunc = f;

    f->next = NULL;
    f->scfunc = func;
    f->menus = menus;
    f->desc = desc;
    f->viewok = viewok;
#ifndef DISABLE_HELP
    f->help = help;
    f->blank_after = blank_after;
#endif

#ifdef DEBUG
    fprintf(stderr, "Added func %ld (%s) for menus %x\n", (long)func, f->desc, menus);
#endif
}

/* Add a key combo to the shortcut list. */
void add_to_sclist(int menus, const char *scstring, void (*func)(void), int toggle)
{
    static sc *tailsc;
    static int counter = 0;
    sc *s = (sc *)nmalloc(sizeof(sc));

    /* Start the list, or tack on the next item. */
    if (sclist == NULL)
	sclist = s;
    else
	tailsc->next = s;
    tailsc = s;
    s->next = NULL;

    /* Fill in the data. */
    s->menus = menus;
    s->scfunc = func;
    s->toggle = toggle;
    if (toggle)
	s->ordinal = ++counter;
    s->keystr = (char *) scstring;
    s->type = strtokeytype(scstring);
    assign_keyinfo(s);

#ifdef DEBUG
    fprintf(stderr, "Setting sequence to %d for shortcut \"%s\" in menus %x\n", s->seq, scstring, s->menus);
#endif
}

/* Assign one function's shortcuts to another function. */
void replace_scs_for(void (*oldfunc)(void), void (*newfunc)(void))
{
    sc *s;

    for (s = sclist; s != NULL; s = s->next)
	if (s->scfunc == oldfunc)
	    s->scfunc = newfunc;
}

/* Return the first shortcut in the list of shortcuts that
 * matches the given func in the given menu. */
const sc *first_sc_for(int menu, void (*func)(void))
{
    const sc *s;

    for (s = sclist; s != NULL; s = s->next)
	if ((s->menus & menu) && s->scfunc == func)
	    return s;

#ifdef DEBUG
    fprintf(stderr, "Whoops, returning null given func %ld in menu %x\n", (long)func, menu);
#endif
    /* Otherwise... */
    return NULL;
}

/* Return the given menu's first shortcut sequence, or the default value
 * (2nd arg).  Assumes currmenu for the menu to check. */
int sc_seq_or(void (*func)(void), int defaultval)
{
    const sc *s = first_sc_for(currmenu, func);

    if (s) {
	meta_key = (s->type == META);
	return s->seq;
    }
    /* else */
    return defaultval;
}

/* Return a pointer to the function that is bound to the given key. */
functionptrtype func_from_key(int *kbinput)
{
    const sc *s = get_shortcut(kbinput);

    if (s)
	return s->scfunc;
    else
	return NULL;
}

/* Return the type of command key based on the given string. */
key_type strtokeytype(const char *str)
{
    if (str[0] == '^')
	return CONTROL;
    else if (str[0] == 'M')
	return META;
    else if (str[0] == 'F')
	return FKEY;
    else
	return RAWINPUT;
}

/* Assign the info to the shortcut struct.
 * Assumes keystr is already assigned, naturally. */
void assign_keyinfo(sc *s)
{
    if (s->type == CONTROL) {
	assert(strlen(s->keystr) > 1);
	s->seq = s->keystr[1] - 64;
    } else if (s->type == META) {
	assert(strlen(s->keystr) > 2);
	s->seq = tolower((int) s->keystr[2]);
    } else if (s->type == FKEY) {
	assert(strlen(s->keystr) > 1);
	s->seq = KEY_F0 + atoi(&s->keystr[1]);
    } else /* RAWINPUT */
	s->seq = (int) s->keystr[0];

    /* Override some keys which don't bind as easily as we'd like. */
    if (s->type == CONTROL && (!strcasecmp(&s->keystr[1], "space")))
	s->seq = 0;
    else if (s->type == META && (!strcasecmp(&s->keystr[2], "space")))
	s->seq = (int) ' ';
    else if (s->type == RAWINPUT) {
	if (!strcasecmp(s->keystr, "Up"))
	    s->seq = KEY_UP;
	else if (!strcasecmp(s->keystr, "Down"))
	    s->seq = KEY_DOWN;
	else if (!strcasecmp(s->keystr, "Left"))
	    s->seq = KEY_LEFT;
	else if (!strcasecmp(s->keystr, "Right"))
	    s->seq = KEY_RIGHT;
	else if (!strcasecmp(s->keystr, "Ins"))
	    s->seq = KEY_IC;
	else if (!strcasecmp(s->keystr, "Del"))
	    s->seq = KEY_DC;
	else if (!strcasecmp(s->keystr, "Bsp"))
	    s->seq = KEY_BACKSPACE;
	/* The Tab and Enter keys don't actually produce special codes
	 * but the exact integer values of ^I and ^M.  Rebinding the
	 * latter therefore also rebinds Tab and Enter. */
	else if (!strcasecmp(s->keystr, "Tab"))
	    s->seq = NANO_CONTROL_I;
	else if (!strcasecmp(s->keystr, "Enter"))
	    s->seq = KEY_ENTER;
	else if (!strcasecmp(s->keystr, "PgUp"))
	    s->seq = KEY_PPAGE;
	else if (!strcasecmp(s->keystr, "PgDn"))
	    s->seq = KEY_NPAGE;
	else if (!strcasecmp(s->keystr, "Home"))
	    s->seq = KEY_HOME;
	else if (!strcasecmp(s->keystr, "End"))
	    s->seq = KEY_END;
    }
}

#ifdef DEBUG
void print_sclist(void)
{
    sc *s;
    const subnfunc *f;

    for (s = sclist; s != NULL; s = s->next) {
	f = sctofunc(s);
	if (f)
	    fprintf(stderr, "Shortcut \"%s\", function: %s, menus %x\n", s->keystr, f->desc, f->menus);
	else
	    fprintf(stderr, "Hmm, didn't find a func for \"%s\"\n", s->keystr);
    }
}
#endif

/* These four tags are used elsewhere too, so they are global. */
/* TRANSLATORS: Try to keep the next fifteen strings at most 10 characters. */
const char *exit_tag = N_("Exit");
const char *close_tag = N_("Close");
const char *uncut_tag = N_("Uncut Text");
#ifndef DISABLE_JUSTIFY
const char *unjust_tag = N_("Unjustify");
#endif

/* Initialize the list of functions and the list of shortcuts. */
void shortcut_init(void)
{
    const char *read_file_tag = N_("Read File");
    const char *whereis_tag = N_("Where Is");
    const char *replace_tag = N_("Replace");
    const char *gotoline_tag = N_("Go To Line");
    const char *prev_line_tag = N_("Prev Line");
    const char *next_line_tag = N_("Next Line");
    const char *prev_page_tag = N_("Prev Page");
    const char *next_page_tag = N_("Next Page");
#ifndef DISABLE_JUSTIFY
    const char *justify_tag = N_("Justify");
    const char *fulljustify_tag = N_("FullJstify");
#endif
    const char *refresh_tag = N_("Refresh");
#ifndef NANO_TINY
    /* TRANSLATORS: Try to keep this string at most 12 characters. */
    const char *whereis_next_tag = N_("WhereIs Next");
#endif

#ifndef DISABLE_HELP
#ifndef DISABLE_JUSTIFY
    /* TRANSLATORS: The next long series of strings are shortcut descriptions;
     * they are best kept shorter than 56 characters, but may be longer. */
    const char *nano_justify_msg = N_("Justify the current paragraph");
#endif
    const char *nano_cancel_msg = N_("Cancel the current function");
    const char *nano_help_msg = N_("Display this help text");
    const char *nano_exit_msg =
#ifndef DISABLE_MULTIBUFFER
	N_("Close the current file buffer / Exit from nano")
#else
	N_("Exit from nano")
#endif
	;
    const char *nano_writeout_msg =
	N_("Write the current file to disk");
    const char *nano_insert_msg =
	N_("Insert another file into the current one");
    const char *nano_whereis_msg =
	N_("Search for a string or a regular expression");
#ifndef DISABLE_BROWSER
    const char *nano_browser_whereis_msg =
	N_("Search for a string");
#endif
    const char *nano_prevpage_msg = N_("Go one screenful up");
    const char *nano_nextpage_msg = N_("Go one screenful down");
    const char *nano_cut_msg =
	N_("Cut the current line and store it in the cutbuffer");
    const char *nano_uncut_msg =
	N_("Uncut from the cutbuffer into the current line");
    const char *nano_cursorpos_msg = N_("Display the position of the cursor");
#ifndef DISABLE_SPELLER
    const char *nano_spell_msg = N_("Invoke the spell checker, if available");
#endif
    const char *nano_replace_msg = N_("Replace a string or a regular expression");
    const char *nano_gotoline_msg = N_("Go to line and column number");
#ifndef NANO_TINY
    const char *nano_mark_msg = N_("Mark text starting from the cursor position");
    const char *nano_whereis_next_msg = N_("Repeat the last search");
    const char *nano_copy_msg =
	N_("Copy the current line and store it in the cutbuffer");
    const char *nano_indent_msg = N_("Indent the current line");
    const char *nano_unindent_msg = N_("Unindent the current line");
    const char *nano_undo_msg = N_("Undo the last operation");
    const char *nano_redo_msg = N_("Redo the last undone operation");
#endif
    const char *nano_back_msg = N_("Go back one character");
    const char *nano_forward_msg = N_("Go forward one character");
#ifndef NANO_TINY
    const char *nano_prevword_msg = N_("Go back one word");
    const char *nano_nextword_msg = N_("Go forward one word");
#endif
    const char *nano_prevline_msg = N_("Go to previous line");
    const char *nano_nextline_msg = N_("Go to next line");
    const char *nano_home_msg = N_("Go to beginning of current line");
    const char *nano_end_msg = N_("Go to end of current line");
#ifndef DISABLE_JUSTIFY
    const char *nano_parabegin_msg =
	N_("Go to beginning of paragraph; then of previous paragraph");
    const char *nano_paraend_msg =
	N_("Go just beyond end of paragraph; then of next paragraph");
#endif
    const char *nano_firstline_msg = N_("Go to the first line of the file");
    const char *nano_lastline_msg = N_("Go to the last line of the file");
#ifndef NANO_TINY
    const char *nano_bracket_msg = N_("Go to the matching bracket");
    const char *nano_scrollup_msg =
	N_("Scroll up one line without scrolling the cursor");
    const char *nano_scrolldown_msg =
	N_("Scroll down one line without scrolling the cursor");
#endif
#ifndef DISABLE_MULTIBUFFER
    const char *nano_prevfile_msg = N_("Switch to the previous file buffer");
    const char *nano_nextfile_msg = N_("Switch to the next file buffer");
#endif
    const char *nano_verbatim_msg = N_("Insert the next keystroke verbatim");
    const char *nano_tab_msg = N_("Insert a tab at the cursor position");
    const char *nano_enter_msg = N_("Insert a newline at the cursor position");
    const char *nano_delete_msg = N_("Delete the character under the cursor");
    const char *nano_backspace_msg =
	N_("Delete the character to the left of the cursor");
#ifndef NANO_TINY
    const char *nano_cut_word_left_msg =
	N_("Cut backward from cursor to word start");
    const char *nano_cut_word_right_msg =
	N_("Cut forward from cursor to next word start");
    const char *nano_cut_till_eof_msg =
	N_("Cut from the cursor position to the end of the file");
#endif
#ifndef DISABLE_JUSTIFY
    const char *nano_fulljustify_msg = N_("Justify the entire file");
#endif
#ifndef NANO_TINY
    const char *nano_wordcount_msg =
	N_("Count the number of words, lines, and characters");
#endif
    const char *nano_refresh_msg =
	N_("Refresh (redraw) the current screen");
    const char *nano_suspend_msg =
	N_("Suspend the editor (if suspension is enabled)");
#ifndef NANO_TINY
    const char *nano_savefile_msg = N_("Save file without prompting");
    const char *nano_findprev_msg = N_("Search next occurrence backward");
    const char *nano_findnext_msg = N_("Search next occurrence forward");
    const char *nano_case_msg =
	N_("Toggle the case sensitivity of the search");
    const char *nano_reverse_msg =
	N_("Reverse the direction of the search");
#endif
#ifdef HAVE_REGEX_H
    const char *nano_regexp_msg =
	N_("Toggle the use of regular expressions");
#endif
#ifndef DISABLE_HISTORIES
    const char *nano_prev_history_msg =
	N_("Recall the previous search/replace string");
    const char *nano_next_history_msg =
	N_("Recall the next search/replace string");
#endif
#ifndef DISABLE_BROWSER
    const char *nano_tofiles_msg = N_("Go to file browser");
#endif
#ifndef NANO_TINY
    const char *nano_dos_msg = N_("Toggle the use of DOS format");
    const char *nano_mac_msg = N_("Toggle the use of Mac format");
    const char *nano_append_msg = N_("Toggle appending");
    const char *nano_prepend_msg = N_("Toggle prepending");
    const char *nano_backup_msg = N_("Toggle backing up of the original file");
    const char *nano_execute_msg = N_("Execute external command");
#endif
    const char *nano_discard_buffer_msg = N_("Close buffer without saving it");
#ifndef DISABLE_MULTIBUFFER
    const char *nano_multibuffer_msg = N_("Toggle the use of a new buffer");
#endif
#ifndef DISABLE_BROWSER
    const char *nano_exitbrowser_msg = N_("Exit from the file browser");
    const char *nano_firstfile_msg = N_("Go to the first file in the list");
    const char *nano_lastfile_msg = N_("Go to the last file in the list");
    const char *nano_backfile_msg = N_("Go to the previous file in the list");
    const char *nano_forwardfile_msg = N_("Go to the next file in the list");
    const char *nano_gotodir_msg = N_("Go to directory");
#endif
#ifndef DISABLE_COLOR
    const char *nano_lint_msg = N_("Invoke the linter, if available");
    const char *nano_prevlint_msg = N_("Go to previous linter msg");
    const char *nano_nextlint_msg = N_("Go to next linter msg");
#ifndef DISABLE_SPELLER
    const char *nano_formatter_msg = N_("Invoke formatter, if available");
#endif
#endif
#endif /* !DISABLE_HELP */

#ifndef DISABLE_HELP
#define IFSCHELP(help) help
#else
#define IFSCHELP(help) ""
#endif

    /* Start populating the different menus with functions. */

    add_to_funcs(do_help_void, MMOST,
	/* TRANSLATORS: Try to keep the following strings at most 10 characters. */
	N_("Get Help"), IFSCHELP(nano_help_msg), TOGETHER, VIEW);

    add_to_funcs(do_cancel, ((MMOST & ~MMAIN & ~MBROWSER) | MYESNO),
	N_("Cancel"), IFSCHELP(nano_cancel_msg), TOGETHER, VIEW);

    add_to_funcs(do_exit, MMAIN,
	exit_tag, IFSCHELP(nano_exit_msg), TOGETHER, VIEW);
    /* Remember the entry for Exit, to be able to replace it with Close. */
    exitfunc = tailfunc;

#ifndef DISABLE_BROWSER
    add_to_funcs(do_exit, MBROWSER,
	exit_tag, IFSCHELP(nano_exitbrowser_msg), TOGETHER, VIEW);
#endif

    add_to_funcs(do_writeout_void, MMAIN,
	N_("Write Out"), IFSCHELP(nano_writeout_msg), TOGETHER, NOVIEW);

#ifndef DISABLE_JUSTIFY
    if (!ISSET(RESTRICTED)) {
#else
    /* If we can't replace Insert with Justify, show Insert anyway, to
     * keep the help items nicely paired also in restricted mode.  */
    if (TRUE) {
#endif
	add_to_funcs(do_insertfile_void, MMAIN,
		read_file_tag, IFSCHELP(nano_insert_msg), BLANKAFTER,
		/* We allow inserting files in view mode if multibuffer mode
		 * is switched on, so that we can view multiple files. */
#ifndef DISABLE_MULTIBUFFER
		VIEW);
#else
		NOVIEW);
#endif
    } else {
#ifndef DISABLE_JUSTIFY
	add_to_funcs(do_justify_void, MMAIN,
		justify_tag, IFSCHELP(nano_justify_msg), BLANKAFTER, NOVIEW);
#endif
    }

    add_to_funcs(do_search, MMAIN,
	whereis_tag, IFSCHELP(nano_whereis_msg), TOGETHER, VIEW);

    add_to_funcs(do_replace, MMAIN,
	replace_tag, IFSCHELP(nano_replace_msg), TOGETHER, NOVIEW);

#ifndef DISABLE_BROWSER
    add_to_funcs(do_search, MBROWSER,
	whereis_tag, IFSCHELP(nano_browser_whereis_msg), TOGETHER, VIEW);

    add_to_funcs(goto_dir_void, MBROWSER,
	N_("Go To Dir"), IFSCHELP(nano_gotodir_msg), BLANKAFTER, VIEW);
#endif

#ifndef DISABLE_HELP
    /* The description ("x") and blank_after (0) are irrelevant,
     * because the help viewer does not have a help text. */
    add_to_funcs(do_exit, MHELP, exit_tag, "x", 0, VIEW);

    add_to_funcs(total_refresh, MHELP, refresh_tag, "x", 0, VIEW);

    add_to_funcs(do_up_void, MHELP, prev_line_tag, "x", 0, VIEW);
    add_to_funcs(do_down_void, MHELP, next_line_tag, "x" , 0, VIEW);
#endif

    add_to_funcs(do_cut_text_void, MMAIN,
	N_("Cut Text"), IFSCHELP(nano_cut_msg), TOGETHER, NOVIEW);

    add_to_funcs(do_uncut_text, MMAIN,
	uncut_tag, IFSCHELP(nano_uncut_msg), BLANKAFTER, NOVIEW);
    /* Remember the entry for Uncut, to be able to replace it with Unjustify. */
    uncutfunc = tailfunc;

    if (!ISSET(RESTRICTED)) {
#ifndef DISABLE_JUSTIFY
	add_to_funcs(do_justify_void, MMAIN,
		justify_tag, IFSCHELP(nano_justify_msg), TOGETHER, NOVIEW);
#endif

#ifndef DISABLE_SPELLER
	add_to_funcs(do_spell, MMAIN,
		N_("To Spell"), IFSCHELP(nano_spell_msg), TOGETHER, NOVIEW);
#endif
#ifndef DISABLE_COLOR
	add_to_funcs(do_linter, MMAIN,
		N_("To Linter"), IFSCHELP(nano_lint_msg), TOGETHER, NOVIEW);
#ifndef DISABLE_SPELLER
	add_to_funcs(do_formatter, MMAIN,
		N_("Formatter"), IFSCHELP(nano_formatter_msg), BLANKAFTER, NOVIEW);
#endif
#endif
    }

#ifndef NANO_TINY
    add_to_funcs(case_sens_void, MWHEREIS|MREPLACE,
	N_("Case Sens"), IFSCHELP(nano_case_msg), TOGETHER, VIEW);
#endif

#ifdef HAVE_REGEX_H
    add_to_funcs(regexp_void, MWHEREIS|MREPLACE,
	N_("Regexp"), IFSCHELP(nano_regexp_msg), TOGETHER, VIEW);
#endif

#ifndef NANO_TINY
    add_to_funcs(backwards_void, MWHEREIS|MREPLACE,
	N_("Backwards"), IFSCHELP(nano_reverse_msg), TOGETHER, VIEW);
#endif

    add_to_funcs(flip_replace_void, MWHEREIS,
	replace_tag, IFSCHELP(nano_replace_msg), TOGETHER, VIEW);

    add_to_funcs(flip_replace_void, MREPLACE,
	N_("No Replace"), IFSCHELP(nano_whereis_msg), TOGETHER, VIEW);

#ifndef DISABLE_JUSTIFY
    add_to_funcs(do_full_justify, MWHEREIS,
	fulljustify_tag, IFSCHELP(nano_fulljustify_msg), TOGETHER, NOVIEW);
#endif

    add_to_funcs(do_cursorpos_void, MMAIN,
	N_("Cur Pos"), IFSCHELP(nano_cursorpos_msg), TOGETHER, VIEW);

#if !defined(NANO_TINY) || defined(DISABLE_COLOR)
    /* Conditionally placing this one here or further on, to keep the
     * help items nicely paired in most conditions. */
    add_to_funcs(do_gotolinecolumn_void, MMAIN|MWHEREIS,
	gotoline_tag, IFSCHELP(nano_gotoline_msg), BLANKAFTER, VIEW);
#endif

    add_to_funcs(do_page_up, MMAIN|MHELP,
	prev_page_tag, IFSCHELP(nano_prevpage_msg), TOGETHER, VIEW);
    add_to_funcs(do_page_down, MMAIN|MHELP,
	next_page_tag, IFSCHELP(nano_nextpage_msg), TOGETHER, VIEW);

    add_to_funcs(do_first_line, MMAIN|MHELP|MWHEREIS|MREPLACE|MREPLACEWITH|MGOTOLINE,
	N_("First Line"), IFSCHELP(nano_firstline_msg), TOGETHER, VIEW);
    add_to_funcs(do_last_line, MMAIN|MHELP|MWHEREIS|MREPLACE|MREPLACEWITH|MGOTOLINE,
	N_("Last Line"), IFSCHELP(nano_lastline_msg), BLANKAFTER, VIEW);

#ifndef NANO_TINY
    add_to_funcs(do_research, MMAIN,
	whereis_next_tag, IFSCHELP(nano_whereis_next_msg), TOGETHER, VIEW);

    add_to_funcs(do_find_bracket, MMAIN,
	N_("To Bracket"), IFSCHELP(nano_bracket_msg), TOGETHER, VIEW);

    add_to_funcs(do_mark, MMAIN,
	N_("Mark Text"), IFSCHELP(nano_mark_msg), TOGETHER, VIEW);

    add_to_funcs(do_copy_text, MMAIN,
	N_("Copy Text"), IFSCHELP(nano_copy_msg), BLANKAFTER, NOVIEW);

    add_to_funcs(do_indent_void, MMAIN,
	N_("Indent Text"), IFSCHELP(nano_indent_msg), TOGETHER, NOVIEW);
    add_to_funcs(do_unindent, MMAIN,
	N_("Unindent Text"), IFSCHELP(nano_unindent_msg), BLANKAFTER, NOVIEW);

    add_to_funcs(do_undo, MMAIN,
	N_("Undo"), IFSCHELP(nano_undo_msg), TOGETHER, NOVIEW);
    add_to_funcs(do_redo, MMAIN,
	N_("Redo"), IFSCHELP(nano_redo_msg), BLANKAFTER, NOVIEW);
#endif /* !NANO_TINY */

    add_to_funcs(do_left, MMAIN,
	N_("Back"), IFSCHELP(nano_back_msg), TOGETHER, VIEW);
    add_to_funcs(do_right, MMAIN,
	N_("Forward"), IFSCHELP(nano_forward_msg), TOGETHER, VIEW);

#ifndef DISABLE_BROWSER
    add_to_funcs(do_left, MBROWSER,
	N_("Back"), IFSCHELP(nano_backfile_msg), TOGETHER, VIEW);
    add_to_funcs(do_right, MBROWSER,
	N_("Forward"), IFSCHELP(nano_forwardfile_msg), TOGETHER, VIEW);
#endif

#ifndef NANO_TINY
    add_to_funcs(do_prev_word_void, MMAIN,
	N_("Prev Word"), IFSCHELP(nano_prevword_msg), TOGETHER, VIEW);
    add_to_funcs(do_next_word_void, MMAIN,
	N_("Next Word"), IFSCHELP(nano_nextword_msg), TOGETHER, VIEW);
#endif

    add_to_funcs(do_home, MMAIN,
	N_("Home"), IFSCHELP(nano_home_msg), TOGETHER, VIEW);
    add_to_funcs(do_end, MMAIN,
	N_("End"), IFSCHELP(nano_end_msg), TOGETHER, VIEW);

    add_to_funcs(do_up_void, MMAIN|MBROWSER,
	prev_line_tag, IFSCHELP(nano_prevline_msg), TOGETHER, VIEW);
    add_to_funcs(do_down_void, MMAIN|MBROWSER,
	next_line_tag, IFSCHELP(nano_nextline_msg), BLANKAFTER, VIEW);

#ifndef DISABLE_JUSTIFY
    add_to_funcs(do_para_begin_void, MMAIN|MWHEREIS,
	N_("Beg of Par"), IFSCHELP(nano_parabegin_msg), TOGETHER, VIEW);
    add_to_funcs(do_para_end_void, MMAIN|MWHEREIS,
	N_("End of Par"), IFSCHELP(nano_paraend_msg), TOGETHER, VIEW);
#endif

#ifndef NANO_TINY
    add_to_funcs(do_scroll_up, MMAIN,
	N_("Scroll Up"), IFSCHELP(nano_scrollup_msg), TOGETHER, VIEW);
    add_to_funcs(do_scroll_down, MMAIN,
	N_("Scroll Down"), IFSCHELP(nano_scrolldown_msg), BLANKAFTER, VIEW);
#endif

#ifndef DISABLE_MULTIBUFFER
    add_to_funcs(switch_to_prev_buffer_void, MMAIN,
	N_("Prev File"), IFSCHELP(nano_prevfile_msg), TOGETHER, VIEW);
    add_to_funcs(switch_to_next_buffer_void, MMAIN,
	N_("Next File"), IFSCHELP(nano_nextfile_msg), BLANKAFTER, VIEW);
#endif

#if defined(NANO_TINY) && !defined(DISABLE_COLOR)
    add_to_funcs(do_gotolinecolumn_void, MMAIN|MWHEREIS,
	gotoline_tag, IFSCHELP(nano_gotoline_msg), BLANKAFTER, VIEW);
#endif

    add_to_funcs(do_verbatim_input, MMAIN,
	N_("Verbatim"), IFSCHELP(nano_verbatim_msg), TOGETHER, NOVIEW);

    add_to_funcs(do_tab, MMAIN,
	N_("Tab"), IFSCHELP(nano_tab_msg), TOGETHER, NOVIEW);
    add_to_funcs(do_enter, MMAIN,
	N_("Enter"), IFSCHELP(nano_enter_msg), BLANKAFTER, NOVIEW);

    add_to_funcs(do_delete, MMAIN,
	N_("Delete"), IFSCHELP(nano_delete_msg), TOGETHER, NOVIEW);
    add_to_funcs(do_backspace, MMAIN,
	N_("Backspace"), IFSCHELP(nano_backspace_msg),
#ifndef NANO_TINY
	TOGETHER,
#else
	BLANKAFTER,
#endif
	NOVIEW);

#ifndef NANO_TINY
    add_to_funcs(do_cut_prev_word, MMAIN,
	/* TRANSLATORS: The next two strings refer to cutting words. */
	N_("Cut Left"), IFSCHELP(nano_cut_word_left_msg), TOGETHER, NOVIEW);
    add_to_funcs(do_cut_next_word, MMAIN,
	N_("Cut Right"), IFSCHELP(nano_cut_word_right_msg), TOGETHER, NOVIEW);
    add_to_funcs(do_cut_till_eof, MMAIN,
	N_("CutTillEnd"), IFSCHELP(nano_cut_till_eof_msg), BLANKAFTER, NOVIEW);
#endif

#ifndef DISABLE_JUSTIFY
    add_to_funcs(do_full_justify, MMAIN,
	fulljustify_tag, IFSCHELP(nano_fulljustify_msg), TOGETHER, NOVIEW);
#endif

#ifndef NANO_TINY
    add_to_funcs(do_wordlinechar_count, MMAIN,
	N_("Word Count"), IFSCHELP(nano_wordcount_msg), TOGETHER, VIEW);
#endif

    add_to_funcs(total_refresh, MMAIN,
	refresh_tag, IFSCHELP(nano_refresh_msg), TOGETHER, VIEW);

    add_to_funcs(do_suspend_void, MMAIN,
	N_("Suspend"), IFSCHELP(nano_suspend_msg), BLANKAFTER, VIEW);

#ifndef NANO_TINY
    add_to_funcs(do_savefile, MMAIN,
	N_("Save"), IFSCHELP(nano_savefile_msg), BLANKAFTER, NOVIEW);

    add_to_funcs(do_findprevious, MMAIN,
	N_("Previous"), IFSCHELP(nano_findprev_msg), TOGETHER, VIEW);
    add_to_funcs(do_findnext, MMAIN,
	N_("Next"), IFSCHELP(nano_findnext_msg), BLANKAFTER, VIEW);
#endif

#ifndef DISABLE_HISTORIES
    add_to_funcs(get_history_older_void,
	(MWHEREIS|MREPLACE|MREPLACEWITH|MWHEREISFILE),
	N_("PrevHstory"), IFSCHELP(nano_prev_history_msg), TOGETHER, VIEW);
    add_to_funcs(get_history_newer_void,
	(MWHEREIS|MREPLACE|MREPLACEWITH|MWHEREISFILE),
	N_("NextHstory"), IFSCHELP(nano_next_history_msg), TOGETHER, VIEW);
#endif

    add_to_funcs(gototext_void, MGOTOLINE,
	N_("Go To Text"), IFSCHELP(nano_whereis_msg), BLANKAFTER, VIEW);

#ifndef NANO_TINY
     add_to_funcs(dos_format_void, MWRITEFILE,
	N_("DOS Format"), IFSCHELP(nano_dos_msg), TOGETHER, NOVIEW);
     add_to_funcs(mac_format_void, MWRITEFILE,
	N_("Mac Format"), IFSCHELP(nano_mac_msg), TOGETHER, NOVIEW);

    /* If we're using restricted mode, the Append, Prepend, and Backup toggles
     * are disabled.  The first and second are not useful as they only allow
     * reduplicating the current file, and the third is not allowed as it
     * would write to a file not specified on the command line. */
    if (!ISSET(RESTRICTED)) {
	add_to_funcs(append_void, MWRITEFILE,
	    N_("Append"), IFSCHELP(nano_append_msg), TOGETHER, NOVIEW);
	add_to_funcs(prepend_void, MWRITEFILE,
	    N_("Prepend"), IFSCHELP(nano_prepend_msg), TOGETHER, NOVIEW);

	add_to_funcs(backup_file_void, MWRITEFILE,
	    N_("Backup File"), IFSCHELP(nano_backup_msg), TOGETHER, NOVIEW);
    }

    /* If we're using restricted mode, file insertion is disabled, and
     * thus command execution and the multibuffer toggle have no place. */
    if (!ISSET(RESTRICTED)) {
	add_to_funcs(flip_execute_void, MINSERTFILE,
	    N_("Execute Command"), IFSCHELP(nano_execute_msg), TOGETHER, NOVIEW);

	add_to_funcs(flip_execute_void, MEXTCMD,
	    read_file_tag, IFSCHELP(nano_insert_msg), TOGETHER, NOVIEW);

#ifndef DISABLE_MULTIBUFFER
	add_to_funcs(new_buffer_void, MINSERTFILE|MEXTCMD,
	    N_("New Buffer"), IFSCHELP(nano_multibuffer_msg), TOGETHER, NOVIEW);
#endif
    }
#endif /* !NANO_TINY */

#ifndef DISABLE_BROWSER
    if (!ISSET(RESTRICTED))
	add_to_funcs(to_files_void, MWRITEFILE|MINSERTFILE,
	    N_("To Files"), IFSCHELP(nano_tofiles_msg), TOGETHER, VIEW);

    add_to_funcs(do_page_up, MBROWSER,
	prev_page_tag, IFSCHELP(nano_prevpage_msg), TOGETHER, VIEW);
    add_to_funcs(do_page_down, MBROWSER,
	next_page_tag, IFSCHELP(nano_nextpage_msg), TOGETHER, VIEW);

    add_to_funcs(do_first_file, (MBROWSER|MWHEREISFILE),
	N_("First File"), IFSCHELP(nano_firstfile_msg), TOGETHER, VIEW);
    add_to_funcs(do_last_file, (MBROWSER|MWHEREISFILE),
	N_("Last File"), IFSCHELP(nano_lastfile_msg), BLANKAFTER, VIEW);
#endif

    add_to_funcs(discard_buffer, MWRITEFILE,
	N_("Discard buffer"), IFSCHELP(nano_discard_buffer_msg), BLANKAFTER, NOVIEW);

#if !defined(NANO_TINY) && !defined(DISABLE_BROWSER)
    add_to_funcs(do_research, MBROWSER,
	whereis_next_tag, IFSCHELP(nano_whereis_next_msg), TOGETHER, VIEW);
#endif

#ifndef DISABLE_COLOR
    add_to_funcs(do_page_up, MLINTER,
	/* TRANSLATORS: Try to keep the next two strings at most 20 characters. */
	N_("Prev Lint Msg"), IFSCHELP(nano_prevlint_msg), TOGETHER, VIEW);
    add_to_funcs(do_page_down, MLINTER,
	N_("Next Lint Msg"), IFSCHELP(nano_nextlint_msg), TOGETHER, VIEW);
#endif

    /* Start associating key combos with functions in specific menus. */

    add_to_sclist(MMOST, "^G", do_help_void, 0);
    add_to_sclist(MMOST, "F1", do_help_void, 0);
    add_to_sclist(MMAIN|MHELP|MBROWSER, "^X", do_exit, 0);
    add_to_sclist(MMAIN|MHELP|MBROWSER, "F2", do_exit, 0);
    add_to_sclist(MMAIN, "^O", do_writeout_void, 0);
    add_to_sclist(MMAIN, "F3", do_writeout_void, 0);
    add_to_sclist(MMAIN, "^R", do_insertfile_void, 0);
    add_to_sclist(MMAIN, "F5", do_insertfile_void, 0);
    add_to_sclist(MMAIN, "Ins", do_insertfile_void, 0);
    add_to_sclist(MMAIN|MBROWSER, "^W", do_search, 0);
    add_to_sclist(MMAIN|MBROWSER, "F6", do_search, 0);
    add_to_sclist(MMAIN, "^\\", do_replace, 0);
    add_to_sclist(MMAIN, "M-R", do_replace, 0);
    add_to_sclist(MMAIN, "F14", do_replace, 0);
    add_to_sclist(MMOST, "^K", do_cut_text_void, 0);
    add_to_sclist(MMOST, "F9", do_cut_text_void, 0);
    add_to_sclist(MMAIN, "^U", do_uncut_text, 0);
    add_to_sclist(MMAIN, "F10", do_uncut_text, 0);
#ifndef DISABLE_JUSTIFY
    add_to_sclist(MMAIN, "^J", do_justify_void, 0);
    add_to_sclist(MMAIN, "F4", do_justify_void, 0);
#endif
#ifndef DISABLE_SPELLER
    add_to_sclist(MMAIN, "^T", do_spell, 0);
    add_to_sclist(MMAIN, "F12", do_spell, 0);
#else
#ifndef DISABLE_COLOR
    add_to_sclist(MMAIN, "^T", do_linter, 0);
    add_to_sclist(MMAIN, "F12", do_linter, 0);
#endif
#endif
    add_to_sclist(MMAIN, "^C", do_cursorpos_void, 0);
    add_to_sclist(MMAIN, "F11", do_cursorpos_void, 0);
    add_to_sclist(MMAIN, "^_", do_gotolinecolumn_void, 0);
    add_to_sclist(MMAIN, "M-G", do_gotolinecolumn_void, 0);
    add_to_sclist(MMAIN, "F13", do_gotolinecolumn_void, 0);
    add_to_sclist(MMAIN|MHELP|MBROWSER|MLINTER, "^Y", do_page_up, 0);
    add_to_sclist(MMAIN|MHELP|MBROWSER|MLINTER, "F7", do_page_up, 0);
    add_to_sclist(MMAIN|MHELP|MBROWSER|MLINTER, "PgUp", do_page_up, 0);
    add_to_sclist(MMAIN|MHELP|MBROWSER|MLINTER, "^V", do_page_down, 0);
    add_to_sclist(MMAIN|MHELP|MBROWSER|MLINTER, "F8", do_page_down, 0);
    add_to_sclist(MMAIN|MHELP|MBROWSER|MLINTER, "PgDn", do_page_down, 0);
    add_to_sclist(MMAIN|MHELP, "M-\\", do_first_line, 0);
    add_to_sclist(MMAIN|MHELP, "M-|", do_first_line, 0);
    add_to_sclist(MMAIN|MHELP, "M-/", do_last_line, 0);
    add_to_sclist(MMAIN|MHELP, "M-?", do_last_line, 0);
#ifndef NANO_TINY
    add_to_sclist(MMAIN|MBROWSER, "M-W", do_research, 0);
    add_to_sclist(MMAIN|MBROWSER, "F16", do_research, 0);
    add_to_sclist(MMAIN, "M-]", do_find_bracket, 0);
    add_to_sclist(MMAIN, "^^", do_mark, 0);
    add_to_sclist(MMAIN, "M-A", do_mark, 0);
    add_to_sclist(MMAIN, "F15", do_mark, 0);
    add_to_sclist(MMAIN, "M-^", do_copy_text, 0);
    add_to_sclist(MMAIN, "M-6", do_copy_text, 0);
    add_to_sclist(MMAIN, "M-}", do_indent_void, 0);
    add_to_sclist(MMAIN, "M-{", do_unindent, 0);
    add_to_sclist(MMAIN, "M-U", do_undo, 0);
    add_to_sclist(MMAIN, "M-E", do_redo, 0);
#endif
    add_to_sclist(MMOST, "^B", do_left, 0);
    add_to_sclist(MMOST, "Left", do_left, 0);
    add_to_sclist(MMOST, "^F", do_right, 0);
    add_to_sclist(MMOST, "Right", do_right, 0);
#ifndef NANO_TINY
    add_to_sclist(MMOST, "M-Space", do_prev_word_void, 0);
    add_to_sclist(MMOST, "^Space", do_next_word_void, 0);
#endif
    add_to_sclist((MMOST & ~MBROWSER), "^A", do_home, 0);
    add_to_sclist((MMOST & ~MBROWSER), "Home", do_home, 0);
    add_to_sclist((MMOST & ~MBROWSER), "^E", do_end, 0);
    add_to_sclist((MMOST & ~MBROWSER), "End", do_end, 0);
    add_to_sclist(MMAIN|MHELP|MBROWSER, "^P", do_up_void, 0);
    add_to_sclist(MMAIN|MHELP|MBROWSER, "Up", do_up_void, 0);
    add_to_sclist(MMAIN|MHELP|MBROWSER, "^N", do_down_void, 0);
    add_to_sclist(MMAIN|MHELP|MBROWSER, "Down", do_down_void, 0);
#ifndef DISABLE_JUSTIFY
    add_to_sclist(MMAIN, "M-(", do_para_begin_void, 0);
    add_to_sclist(MMAIN, "M-9", do_para_begin_void, 0);
    add_to_sclist(MMAIN, "M-)", do_para_end_void, 0);
    add_to_sclist(MMAIN, "M-0", do_para_end_void, 0);
#endif
#ifndef NANO_TINY
    add_to_sclist(MMAIN, "M--", do_scroll_up, 0);
    add_to_sclist(MMAIN, "M-_", do_scroll_up, 0);
    add_to_sclist(MMAIN, "M-+", do_scroll_down, 0);
    add_to_sclist(MMAIN, "M-=", do_scroll_down, 0);
#endif
#ifndef DISABLE_MULTIBUFFER
    add_to_sclist(MMAIN, "M-<", switch_to_prev_buffer_void, 0);
    add_to_sclist(MMAIN, "M-,", switch_to_prev_buffer_void, 0);
    add_to_sclist(MMAIN, "M->", switch_to_next_buffer_void, 0);
    add_to_sclist(MMAIN, "M-.", switch_to_next_buffer_void, 0);
#endif
    add_to_sclist(MMOST, "M-V", do_verbatim_input, 0);
#ifndef NANO_TINY
    add_to_sclist(MMAIN, "M-T", do_cut_till_eof, 0);
    add_to_sclist(MMAIN, "M-D", do_wordlinechar_count, 0);
#endif
#ifndef DISABLE_JUSTIFY
    add_to_sclist(MMAIN|MWHEREIS, "M-J", do_full_justify, 0);
#endif
    add_to_sclist(MMAIN|MHELP, "^L", total_refresh, 0);
    add_to_sclist(MMAIN, "^Z", do_suspend_void, 0);

#ifndef NANO_TINY
    /* Group of "Appearance" toggles. */
    add_to_sclist(MMAIN, "M-X", do_toggle_void, NO_HELP);
    add_to_sclist(MMAIN, "M-C", do_toggle_void, CONST_UPDATE);
    add_to_sclist(MMAIN, "M-O", do_toggle_void, MORE_SPACE);
    add_to_sclist(MMAIN, "M-S", do_toggle_void, SMOOTH_SCROLL);
    add_to_sclist(MMAIN, "M-$", do_toggle_void, SOFTWRAP);
    add_to_sclist(MMAIN, "M-P", do_toggle_void, WHITESPACE_DISPLAY);
#ifndef DISABLE_COLOR
    add_to_sclist(MMAIN, "M-Y", do_toggle_void, NO_COLOR_SYNTAX);
#endif

    /* Group of "Editing-behavior" toggles. */
    add_to_sclist(MMAIN, "M-H", do_toggle_void, SMART_HOME);
    add_to_sclist(MMAIN, "M-I", do_toggle_void, AUTOINDENT);
    add_to_sclist(MMAIN, "M-K", do_toggle_void, CUT_TO_END);
#ifndef DISABLE_WRAPPING
    add_to_sclist(MMAIN, "M-L", do_toggle_void, NO_WRAP);
#endif
    add_to_sclist(MMAIN, "M-Q", do_toggle_void, TABS_TO_SPACES);

    /* Group of "Peripheral-feature" toggles. */
    add_to_sclist(MMAIN, "M-B", do_toggle_void, BACKUP_FILE);
#ifndef DISABLE_MULTIBUFFER
    add_to_sclist(MMAIN, "M-F", do_toggle_void, MULTIBUFFER);
#endif
#ifndef DISABLE_MOUSE
    add_to_sclist(MMAIN, "M-M", do_toggle_void, USE_MOUSE);
#endif
    add_to_sclist(MMAIN, "M-N", do_toggle_void, NO_CONVERT);
    add_to_sclist(MMAIN, "M-Z", do_toggle_void, SUSPEND);
#endif /* !NANO_TINY */

    add_to_sclist(MMAIN, "^Q", xon_complaint, 0);
    add_to_sclist(MMAIN, "^S", xoff_complaint, 0);

    add_to_sclist(((MMOST & ~MMAIN & ~MBROWSER) | MYESNO), "^C", do_cancel, 0);

#ifndef NANO_TINY
    add_to_sclist(MWHEREIS|MREPLACE, "M-B", backwards_void, 0);
    add_to_sclist(MWHEREIS|MREPLACE, "M-C", case_sens_void, 0);
#endif
    add_to_sclist(MWHEREIS|MREPLACE, "M-R", regexp_void, 0);
    add_to_sclist(MWHEREIS|MREPLACE, "^R", flip_replace_void, 0);
    add_to_sclist(MWHEREIS|MREPLACE|MREPLACEWITH|MGOTOLINE, "^Y", do_first_line, 0);
    add_to_sclist(MWHEREIS|MREPLACE|MREPLACEWITH|MGOTOLINE, "^V", do_last_line, 0);
#ifndef DISABLE_JUSTIFY
    add_to_sclist(MWHEREIS|MREPLACE|MREPLACEWITH, "^W", do_para_begin_void, 0);
    add_to_sclist(MWHEREIS|MREPLACE|MREPLACEWITH, "^O", do_para_end_void, 0);
#endif
    add_to_sclist(MWHEREIS, "^T", do_gotolinecolumn_void, 0);
    add_to_sclist(MGOTOLINE, "^T", gototext_void, 0);
#ifndef DISABLE_HISTORIES
    add_to_sclist(MWHEREIS|MREPLACE|MREPLACEWITH|MWHEREISFILE, "^P", get_history_older_void, 0);
    add_to_sclist(MWHEREIS|MREPLACE|MREPLACEWITH|MWHEREISFILE, "Up", get_history_older_void, 0);
    add_to_sclist(MWHEREIS|MREPLACE|MREPLACEWITH|MWHEREISFILE, "^N", get_history_newer_void, 0);
    add_to_sclist(MWHEREIS|MREPLACE|MREPLACEWITH|MWHEREISFILE, "Down", get_history_newer_void, 0);
#endif
#ifndef DISABLE_BROWSER
    add_to_sclist(MWHEREISFILE, "^Y", do_first_file, 0);
    add_to_sclist(MWHEREISFILE, "^V", do_last_file, 0);
    add_to_sclist(MBROWSER|MWHEREISFILE, "M-\\", do_first_file, 0);
    add_to_sclist(MBROWSER|MWHEREISFILE, "M-|", do_first_file, 0);
    add_to_sclist(MBROWSER|MWHEREISFILE, "M-/", do_last_file, 0);
    add_to_sclist(MBROWSER|MWHEREISFILE, "M-?", do_last_file, 0);
    add_to_sclist(MBROWSER, "Home", do_first_file, 0);
    add_to_sclist(MBROWSER, "End", do_last_file, 0);
    add_to_sclist(MBROWSER, "^_", goto_dir_void, 0);
    add_to_sclist(MBROWSER, "M-G", goto_dir_void, 0);
    add_to_sclist(MBROWSER, "F13", goto_dir_void, 0);
#endif
    if (ISSET(TEMP_FILE))
	add_to_sclist(MWRITEFILE, "^Q", discard_buffer, 0);
    add_to_sclist(MWRITEFILE, "M-D", dos_format_void, 0);
    add_to_sclist(MWRITEFILE, "M-M", mac_format_void, 0);
    if (!ISSET(RESTRICTED)) {
	/* Don't allow Appending, Prepending, nor Backups in restricted mode. */
	add_to_sclist(MWRITEFILE, "M-A", append_void, 0);
	add_to_sclist(MWRITEFILE, "M-P", prepend_void, 0);
	add_to_sclist(MWRITEFILE, "M-B", backup_file_void, 0);
#ifndef DISABLE_BROWSER
	add_to_sclist(MWRITEFILE|MINSERTFILE, "^T", to_files_void, 0);
#endif
	add_to_sclist(MINSERTFILE|MEXTCMD, "^X", flip_execute_void, 0);
	add_to_sclist(MINSERTFILE|MEXTCMD, "M-F", new_buffer_void, 0);
    }
    add_to_sclist(MHELP|MBROWSER, "^C", do_exit, 0);
    /* Allow exiting from the file browser and the help viewer with
     * the same key as they were entered. */
#ifndef DISABLE_BROWSER
    add_to_sclist(MBROWSER, "^T", do_exit, 0);
#endif
#ifndef DISABLE_HELP
    add_to_sclist(MHELP, "^G", do_exit, 0);
    add_to_sclist(MHELP, "Home", do_first_line, 0);
    add_to_sclist(MHELP, "End", do_last_line, 0);
#endif
    add_to_sclist(MMOST, "^I", do_tab, 0);
    add_to_sclist(MMOST, "Tab", do_tab, 0);
    add_to_sclist(MMOST, "^M", do_enter, 0);
    add_to_sclist(MMOST, "Enter", do_enter, 0);
    add_to_sclist(MMOST, "^D", do_delete, 0);
    add_to_sclist(MMOST, "Del", do_delete, 0);
    add_to_sclist(MMOST, "^H", do_backspace, 0);
    add_to_sclist(MMOST, "Bsp", do_backspace, 0);

#ifdef DEBUG
    print_sclist();
#endif
}

#ifndef DISABLE_COLOR
void set_lint_or_format_shortcuts(void)
{
#ifndef DISABLE_SPELLER
    if (openfile->syntax->formatter) {
	replace_scs_for(do_spell, do_formatter);
	replace_scs_for(do_linter, do_formatter);
    } else {
	replace_scs_for(do_spell, do_linter);
	replace_scs_for(do_formatter, do_linter);
    }
#endif
}

void set_spell_shortcuts(void)
{
#ifndef DISABLE_SPELLER
	replace_scs_for(do_formatter, do_spell);
	replace_scs_for(do_linter, do_spell);
#endif
}
#endif /* !DISABLE_COLOR */

const subnfunc *sctofunc(const sc *s)
{
    subnfunc *f = allfuncs;

    while (f != NULL && f->scfunc != s->scfunc)
	f = f->next;

    return f;
}

#ifndef NANO_TINY
/* Now let's come up with a single (hopefully) function to get a string
 * for each flag. */
const char *flagtostr(int flag)
{
    switch (flag) {
	case NO_HELP:
	    /* TRANSLATORS: The next seventeen strings are toggle descriptions;
	     * they are best kept shorter than 40 characters, but may be longer. */
	    return N_("Help mode");
	case CONST_UPDATE:
	    return N_("Constant cursor position display");
	case MORE_SPACE:
	    return N_("Use of one more line for editing");
	case SMOOTH_SCROLL:
	    return N_("Smooth scrolling");
	case SOFTWRAP:
	    return N_("Soft wrapping of overlong lines");
	case WHITESPACE_DISPLAY:
	    return N_("Whitespace display");
	case NO_COLOR_SYNTAX:
	    return N_("Color syntax highlighting");
	case SMART_HOME:
	    return N_("Smart home key");
	case AUTOINDENT:
	    return N_("Auto indent");
	case CUT_TO_END:
	    return N_("Cut to end");
	case NO_WRAP:
	    return N_("Hard wrapping of overlong lines");
	case TABS_TO_SPACES:
	    return N_("Conversion of typed tabs to spaces");
	case BACKUP_FILE:
	    return N_("Backup files");
	case MULTIBUFFER:
	    return N_("Reading file into separate buffer");
	case USE_MOUSE:
	    return N_("Mouse support");
	case NO_CONVERT:
	    return N_("No conversion from DOS/Mac format");
	case SUSPEND:
	    return N_("Suspension");
	default:
	    return "?????";
    }
}
#endif /* !NANO_TINY */

#ifndef DISABLE_NANORC
/* Interpret a function string given in the rc file, and return a
 * shortcut struct with the corresponding function filled in. */
sc *strtosc(const char *input)
{
    sc *s;

    s = (sc *)nmalloc(sizeof(sc));

#ifndef DISABLE_HELP
    if (!strcasecmp(input, "help"))
	s->scfunc = do_help_void;
    else
#endif
    if (!strcasecmp(input, "cancel"))
	s->scfunc = do_cancel;
    else if (!strcasecmp(input, "exit"))
	s->scfunc = do_exit;
    else if (!strcasecmp(input, "discardbuffer"))
	s->scfunc = discard_buffer;
    else if (!strcasecmp(input, "writeout"))
	s->scfunc = do_writeout_void;
#ifndef NANO_TINY
    else if (!strcasecmp(input, "savefile"))
	s->scfunc = do_savefile;
#endif
    else if (!strcasecmp(input, "insert"))
	s->scfunc = do_insertfile_void;
    else if (!strcasecmp(input, "whereis"))
	s->scfunc = do_search;
#ifndef NANO_TINY
    else if (!strcasecmp(input, "searchagain") ||
	     !strcasecmp(input, "research"))
	s->scfunc = do_research;
    else if (!strcasecmp(input, "findprevious"))
	s->scfunc = do_findprevious;
    else if (!strcasecmp(input, "findnext"))
	s->scfunc = do_findnext;
#endif
    else if (!strcasecmp(input, "replace"))
	s->scfunc = do_replace;
    else if (!strcasecmp(input, "cut"))
	s->scfunc = do_cut_text_void;
    else if (!strcasecmp(input, "uncut"))
	s->scfunc = do_uncut_text;
#ifndef NANO_TINY
    else if (!strcasecmp(input, "cutrestoffile"))
	s->scfunc = do_cut_till_eof;
    else if (!strcasecmp(input, "copytext"))
	s->scfunc = do_copy_text;
    else if (!strcasecmp(input, "mark"))
	s->scfunc = do_mark;
#endif
#ifndef DISABLE_SPELLER
    else if (!strcasecmp(input, "tospell") ||
	     !strcasecmp(input, "speller"))
	s->scfunc = do_spell;
#endif
#ifndef DISABLE_COLOR
    else if (!strcasecmp(input, "linter"))
	s->scfunc = do_linter;
#endif
    else if (!strcasecmp(input, "curpos") ||
	     !strcasecmp(input, "cursorpos"))
	s->scfunc = do_cursorpos_void;
    else if (!strcasecmp(input, "gotoline"))
	s->scfunc = do_gotolinecolumn_void;
#ifndef DISABLE_JUSTIFY
    else if (!strcasecmp(input, "justify"))
	s->scfunc = do_justify_void;
    else if (!strcasecmp(input, "fulljustify"))
	s->scfunc = do_full_justify;
    else if (!strcasecmp(input, "beginpara"))
	s->scfunc = do_para_begin_void;
    else if (!strcasecmp(input, "endpara"))
	s->scfunc = do_para_end_void;
#endif
#ifndef NANO_TINY
    else if (!strcasecmp(input, "indent"))
	s->scfunc = do_indent_void;
    else if (!strcasecmp(input, "unindent"))
	s->scfunc = do_unindent;
    else if (!strcasecmp(input, "scrollup"))
	s->scfunc = do_scroll_up;
    else if (!strcasecmp(input, "scrolldown"))
	s->scfunc = do_scroll_down;
    else if (!strcasecmp(input, "prevword"))
	s->scfunc = do_prev_word_void;
    else if (!strcasecmp(input, "nextword"))
	s->scfunc = do_next_word_void;
    else if (!strcasecmp(input, "cutwordleft"))
	s->scfunc = do_cut_prev_word;
    else if (!strcasecmp(input, "cutwordright"))
	s->scfunc = do_cut_next_word;
    else if (!strcasecmp(input, "findbracket"))
	s->scfunc = do_find_bracket;
    else if (!strcasecmp(input, "wordcount"))
	s->scfunc = do_wordlinechar_count;
    else if (!strcasecmp(input, "undo"))
	s->scfunc = do_undo;
    else if (!strcasecmp(input, "redo"))
	s->scfunc = do_redo;
#endif
    else if (!strcasecmp(input, "left") ||
	     !strcasecmp(input, "back"))
	s->scfunc = do_left;
    else if (!strcasecmp(input, "right") ||
	     !strcasecmp(input, "forward"))
	s->scfunc = do_right;
    else if (!strcasecmp(input, "up") ||
	     !strcasecmp(input, "prevline"))
	s->scfunc = do_up_void;
    else if (!strcasecmp(input, "down") ||
	     !strcasecmp(input, "nextline"))
	s->scfunc = do_down_void;
    else if (!strcasecmp(input, "home"))
	s->scfunc = do_home;
    else if (!strcasecmp(input, "end"))
	s->scfunc = do_end;
    else if (!strcasecmp(input, "pageup") ||
	     !strcasecmp(input, "prevpage"))
	s->scfunc = do_page_up;
    else if (!strcasecmp(input, "pagedown") ||
	     !strcasecmp(input, "nextpage"))
	s->scfunc = do_page_down;
    else if (!strcasecmp(input, "firstline"))
	s->scfunc = do_first_line;
    else if (!strcasecmp(input, "lastline"))
	s->scfunc = do_last_line;
#ifndef DISABLE_MULTIBUFFER
    else if (!strcasecmp(input, "prevbuf"))
	s->scfunc = switch_to_prev_buffer_void;
    else if (!strcasecmp(input, "nextbuf"))
	s->scfunc = switch_to_next_buffer_void;
#endif
    else if (!strcasecmp(input, "verbatim"))
	s->scfunc = do_verbatim_input;
    else if (!strcasecmp(input, "tab"))
	s->scfunc = do_tab;
    else if (!strcasecmp(input, "enter"))
	s->scfunc = do_enter;
    else if (!strcasecmp(input, "delete"))
	s->scfunc = do_delete;
    else if (!strcasecmp(input, "backspace"))
	s->scfunc = do_backspace;
    else if (!strcasecmp(input, "refresh"))
	s->scfunc = total_refresh;
    else if (!strcasecmp(input, "suspend"))
	s->scfunc = do_suspend_void;
    else if (!strcasecmp(input, "casesens"))
	s->scfunc = case_sens_void;
#ifndef NANO_TINY
    else if (!strcasecmp(input, "regexp") ||
	     !strcasecmp(input, "regex"))
	s->scfunc = regexp_void;
    else if (!strcasecmp(input, "backwards"))
	s->scfunc = backwards_void;
#endif
    else if (!strcasecmp(input, "flipreplace") ||
	     !strcasecmp(input, "dontreplace"))
	s->scfunc = flip_replace_void;
    else if (!strcasecmp(input, "gototext"))
	s->scfunc = gototext_void;
#ifndef DISABLE_HISTORIES
    else if (!strcasecmp(input, "prevhistory"))
	s->scfunc = get_history_older_void;
    else if (!strcasecmp(input, "nexthistory"))
	s->scfunc = get_history_newer_void;
#endif
    else if (!strcasecmp(input, "dosformat"))
	s->scfunc = dos_format_void;
    else if (!strcasecmp(input, "macformat"))
	s->scfunc = mac_format_void;
    else if (!strcasecmp(input, "append"))
	s->scfunc = append_void;
    else if (!strcasecmp(input, "prepend"))
	s->scfunc = prepend_void;
    else if (!strcasecmp(input, "backup"))
	s->scfunc = backup_file_void;
#ifndef ENABLE_TINY
    else if (!strcasecmp(input, "flipexecute"))
	s->scfunc = flip_execute_void;
#endif
#ifndef DISABLE_MULTIBUFFER
    else if (!strcasecmp(input, "flipnewbuffer") ||
	     !strcasecmp(input, "newbuffer"))
	s->scfunc = new_buffer_void;
#endif
#ifndef DISABLE_BROWSER
    else if (!strcasecmp(input, "tofiles") ||
	     !strcasecmp(input, "browser"))
	s->scfunc = to_files_void;
    else if (!strcasecmp(input, "gotodir"))
	s->scfunc = goto_dir_void;
    else if (!strcasecmp(input, "firstfile"))
	s->scfunc = do_first_file;
    else if (!strcasecmp(input, "lastfile"))
	s->scfunc = do_last_file;
#endif
#ifndef NANO_TINY
    else {
	s->scfunc = do_toggle_void;
	if (!strcasecmp(input, "nohelp"))
	    s->toggle = NO_HELP;
	else if (!strcasecmp(input, "constupdate"))
	    s->toggle = CONST_UPDATE;
	else if (!strcasecmp(input, "morespace"))
	    s->toggle = MORE_SPACE;
	else if (!strcasecmp(input, "smoothscroll"))
	    s->toggle = SMOOTH_SCROLL;
	else if (!strcasecmp(input, "softwrap"))
	    s->toggle = SOFTWRAP;
	else if (!strcasecmp(input, "whitespacedisplay"))
	    s->toggle = WHITESPACE_DISPLAY;
#ifndef DISABLE_COLOR
	else if (!strcasecmp(input, "nosyntax"))
	    s->toggle = NO_COLOR_SYNTAX;
#endif
	else if (!strcasecmp(input, "smarthome"))
	    s->toggle = SMART_HOME;
	else if (!strcasecmp(input, "autoindent"))
	    s->toggle = AUTOINDENT;
	else if (!strcasecmp(input, "cuttoend"))
	    s->toggle = CUT_TO_END;
#ifndef DISABLE_WRAPPING
	else if (!strcasecmp(input, "nowrap"))
	    s->toggle = NO_WRAP;
#endif
	else if (!strcasecmp(input, "tabstospaces"))
	    s->toggle = TABS_TO_SPACES;
	else if (!strcasecmp(input, "backupfile"))
	    s->toggle = BACKUP_FILE;
#ifndef DISABLE_MULTIBUFFER
	else if (!strcasecmp(input, "multibuffer"))
	    s->toggle = MULTIBUFFER;
#endif
#ifndef DISABLE_MOUSE
	else if (!strcasecmp(input, "mouse"))
	    s->toggle = USE_MOUSE;
#endif
	else if (!strcasecmp(input, "noconvert"))
	    s->toggle = NO_CONVERT;
	else if (!strcasecmp(input, "suspendenable"))
	    s->toggle = SUSPEND;
#endif /* !NANO_TINY */
	else {
	    free(s);
	    return NULL;
	}
#ifndef NANO_TINY
    }
#endif
    return s;
}

/* Interpret a menu name and return the corresponding menu flag. */
int strtomenu(const char *input)
{
    if (!strcasecmp(input, "all"))
	return (MMOST|MHELP|MYESNO);
    else if (!strcasecmp(input, "main"))
	return MMAIN;
    else if (!strcasecmp(input, "search"))
	return MWHEREIS;
    else if (!strcasecmp(input, "replace"))
	return MREPLACE;
    else if (!strcasecmp(input, "replace2") ||
	     !strcasecmp(input, "replacewith"))
	return MREPLACEWITH;
    else if (!strcasecmp(input, "gotoline"))
	return MGOTOLINE;
    else if (!strcasecmp(input, "writeout"))
	return MWRITEFILE;
    else if (!strcasecmp(input, "insert"))
	return MINSERTFILE;
    else if (!strcasecmp(input, "externalcmd") ||
	     !strcasecmp(input, "extcmd"))
	return MEXTCMD;
#ifndef DISABLE_HELP
    else if (!strcasecmp(input, "help"))
	return MHELP;
#endif
#ifndef DISABLE_SPELLER
    else if (!strcasecmp(input, "spell"))
	return MSPELL;
#endif
    else if (!strcasecmp(input, "linter"))
	return MLINTER;
#ifndef DISABLE_BROWSER
    else if (!strcasecmp(input, "browser"))
	return MBROWSER;
    else if (!strcasecmp(input, "whereisfile"))
	return MWHEREISFILE;
    else if (!strcasecmp(input, "gotodir"))
	return MGOTODIR;
#endif
    return -1;
}
#endif /* !DISABLE_NANORC */


#ifdef DEBUG
#ifndef DISABLE_COLOR
void free_list_item(regexlisttype *dropit)
{
    free(dropit->ext_regex);
    if (dropit->ext != NULL)
	regfree(dropit->ext);
    free(dropit->ext);
    free(dropit);
}
#endif

/* This function is used to gracefully return all the memory we've used.
 * It should be called just before calling exit().  Practically, the
 * only effect is to cause a segmentation fault if the various data
 * structures got bolloxed earlier.  Thus, we don't bother having this
 * function unless debugging is turned on. */
void thanks_for_all_the_fish(void)
{
    delwin(topwin);
    delwin(edit);
    delwin(bottomwin);

#ifndef DISABLE_JUSTIFY
    free(quotestr);
#ifdef HAVE_REGEX_H
    regfree(&quotereg);
    free(quoteerr);
#endif
#endif
#ifndef NANO_TINY
    free(backup_dir);
#endif
#ifndef DISABLE_OPERATINGDIR
    free(operating_dir);
    free(full_operating_dir);
#endif
    free(answer);
    free(last_search);
    free(last_replace);
#ifndef DISABLE_SPELLER
    free(alt_speller);
#endif
    free_filestruct(cutbuffer);
#ifndef DISABLE_JUSTIFY
    free_filestruct(jusbuffer);
#endif
    /* Free the memory associated with each open file buffer. */
    while (openfile != openfile->next) {
	openfile = openfile->next;
	delete_opennode(openfile->prev);
    }
    delete_opennode(openfile);
#ifndef DISABLE_COLOR
    free(syntaxstr);
    while (syntaxes != NULL) {
	syntaxtype *bill = syntaxes;

	free(syntaxes->desc);
	free(syntaxes->linter);
	free(syntaxes->formatter);

	while (syntaxes->extensions != NULL) {
	    regexlisttype *bob = syntaxes->extensions;
	    syntaxes->extensions = bob->next;
	    free_list_item(bob);
	}
	while (syntaxes->headers != NULL) {
	    regexlisttype *bob = syntaxes->headers;
	    syntaxes->headers = bob->next;
	    free_list_item(bob);
	}
	while (syntaxes->magics != NULL) {
	    regexlisttype *bob = syntaxes->magics;
	    syntaxes->magics = bob->next;
	    free_list_item(bob);
	}

	while (syntaxes->color != NULL) {
	    colortype *bob = syntaxes->color;
	    syntaxes->color = bob->next;
	    free(bob->start_regex);
	    if (bob->start != NULL) {
		regfree(bob->start);
		free(bob->start);
	    }
	    free(bob->end_regex);
	    if (bob->end != NULL) {
		regfree(bob->end);
		free(bob->end);
	    }
	    free(bob);
	}
	syntaxes = syntaxes->next;
	free(bill);
    }
#endif /* !DISABLE_COLOR */
#ifndef DISABLE_HISTORIES
    /* Free the search and replace history lists. */
    free_filestruct(searchage);
    free_filestruct(replaceage);
#endif
    /* Free the list of functions. */
    while (allfuncs != NULL) {
	subnfunc *f = allfuncs;
	allfuncs = allfuncs->next;
	free(f);
    }
    /* Free the list of shortcuts. */
    while (sclist != NULL) {
	sc *s = sclist;
	sclist = sclist->next;
	free(s);
    }
#ifndef DISABLE_NANORC
    free(homedir);
#endif
}
#endif /* DEBUG */
