/* setterm.c, set terminal attributes.
 *
 * Copyright (C) 1990 Gordon Irlam (gordoni@cs.ua.oz.au).  Conditions of use,
 * modification, and redistribution are contained in the file COPYRIGHT that
 * forms part of this distribution.
 *
 *
 * Syntax:
 *
 * setterm
 *   [ -term terminal_name ]
 *   [ -reset ]
 *   [ -initialize ]
 *   [ -cursor [on|off] ]
 *   [ -keyboard pc|olivetti|dutch|extended ]
 *   [ -linewrap [on|off] ]
 *   [ -snow [on|off] ]
 *   [ -softscroll [on|off] ]
 *   [ -default ]
 *   [ -foreground black|red|green|yellow|blue|magenta|cyan|white|default ]
 *   [ -background black|red|green|yellow|blue|magenta|cyan|white|default ]
 *   [ -bold [on|off] ]
 *   [ -blink [on|off] ]
 *   [ -reverse [on|off] ]
 *   [ -underline [on|off] ]
 *   [ -store ]
 *   [ -clear [all|rest] ]
 *   [ -blank [0-60]
 *
 *
 * Semantics:
 *
 * Setterm writes to standard output a character string that will invoke the
 * specified terminal capabilities.  Where possibile termcap is consulted to
 * find the string to use.  Some options however do not corresponding to a
 * termcap capability.  In this case if the terminal type is "minix-vc", or
 * "minix-vcam" the string that invokes the specified capabilities on the PC
 * Minix virtual console driver is output.  Options that are not implemented
 * by the terminal are ignored.
 *
 * The following options are non-obvious.
 *
 *   -term can be used to override the TERM environment variable.
 *
 *   -reset displays the terminal reset string, which typically resets the
 *      terminal to its power on state.
 *
 *   -initialize displays the terminal initialization string, which typically
 *      sets the terminal's rendering options, and other attributes to the
 *      default values.
 *
 *   -default sets the terminal's rendering options to the default values.
 *
 *   -store stores the terminal's current rendering options as the default
 *      values.
 */

#include<stdlib.h>
#include<stdio.h>
#include<ctype.h>
#include<termcap.h>

/* Constants. */

/* Known special terminal types. */
#define VCTERM    "console"
#define VCTERM_AM "console50"

/* Termcap constants. */
#define TC_BUF_SIZE 1024	/* Size of termcap(3) buffer. */
#define TC_ENT_SIZE 50		/* Size of termcap(3) entry buffer. */

/* General constants. */
#define TRUE  1
#define FALSE 0

/* Keyboard types. */
#define PC	 0
#define OLIVETTI 1
#define DUTCH    2
#define EXTENDED 3

/* Colors. */
/*
#define BLACK   0
#define RED     1
#define GREEN   2
#define YELLOW  3
#define BLUE    4
#define MAGENTA 5
#define CYAN    6
#define WHITE   7
#define DEFAULT 9 */
/* Colors. */
#define BLACK   0
#define BLUE    1
#define GREEN   2
#define CYAN 	3
#define RED     4
#define MAGENTA 5
#define YELLOW  6
#define WHITE   7
#define DEFAULT 9

/* Control sequences. */
#define ESC "\033"
#define DCS "\033P"
#define ST  "\033\\"

/* Static variables. */

char tc_buf[TC_BUF_SIZE];	/* Termcap buffer. */

/* Option flags.  Set if the option is to be invoked. */
int opt_term, opt_reset, opt_initialize, opt_cursor, opt_keyboard;
int opt_linewrap, opt_snow, opt_softscroll, opt_default, opt_foreground;
int opt_background, opt_bold, opt_blink, opt_reverse, opt_underline;
int opt_store, opt_clear, opt_blank;

/* Option controls.  The variable names have been contracted to ensure
 * uniqueness.
 */
char *opt_te_terminal_name;	/* Terminal name. */
int opt_cu_on, opt_li_on, opt_sn_on, opt_so_on, opt_bo_on, opt_bl_on;
int opt_re_on, opt_un_on;	/* Boolean switches. */
int opt_ke_type;		/* Keyboard type. */
int opt_fo_color, opt_ba_color;	/* Colors. */
int opt_cl_all;			/* Clear all or rest. */
int opt_bl_min;			/* Blank screen. */

/* Command line parsing routines.
 *
 * Note that it is an error for a given option to be invoked more than once.
 */

void parse_term(argc, argv, option, opt_term, bad_arg)
int argc;			/* Number of arguments for this option. */
char *argv[];			/* Arguments for this option. */
int *option;			/* Term flag to set. */
char **opt_term;		/* Terminal name to set. */
int *bad_arg;			/* Set to true if an error is detected. */
{
/* Parse a -term specification. */

  if (argc != 1 || *option) *bad_arg = TRUE;
  *option = TRUE;
  if (argc == 1) {
	*opt_term = argv[0];
  }
}

void parse_none(argc, argv, option, bad_arg)
int argc;			/* Number of arguments for this option. */
char *argv[];			/* Arguments for this option. */
int *option;			/* Option flag to set. */
int *bad_arg;			/* Set to true if an error is detected. */
{
/* Parse a parameterless specification. */

  if (argc != 0 || *option) *bad_arg = TRUE;
  *option = TRUE;
}

void parse_switch(argc, argv, option, opt_on, bad_arg)
int argc;			/* Number of arguments for this option. */
char *argv[];			/* Arguments for this option. */
int *option;			/* Option flag to set. */
int *opt_on;			/* Boolean option switch to set or reset. */
int *bad_arg;			/* Set to true if an error is detected. */
{
/* Parse a boolean (on/off) specification. */

  if (argc > 1 || *option) *bad_arg = TRUE;
  *option = TRUE;
  if (argc == 1) {
	if (strcmp(argv[0], "on") == 0)
		*opt_on = TRUE;
	else if (strcmp(argv[0], "off") == 0)
		*opt_on = FALSE;
	else
		*bad_arg = TRUE;
  } else {
	*opt_on = TRUE;
  }
}

void parse_keyboard(argc, argv, option, opt_keyboard, bad_arg)
int argc;			/* Number of arguments for this option. */
char *argv[];			/* Arguments for this option. */
int *option;			/* Keyboard flag to set. */
int *opt_keyboard;		/* Keyboard type to set. */
int *bad_arg;			/* Set to true if an error is detected. */
{
/* Parse a -keyboard specification. */

  if (argc != 1 || *option) *bad_arg = TRUE;
  *option = TRUE;
  if (argc == 1) {
	if (strcmp(argv[0], "pc") == 0)
		*opt_keyboard = PC;
	else if (strcmp(argv[0], "olivetti") == 0)
		*opt_keyboard = OLIVETTI;
	else if (strcmp(argv[0], "dutch") == 0)
		*opt_keyboard = DUTCH;
	else if (strcmp(argv[0], "extended") == 0)
		*opt_keyboard = EXTENDED;
	else
		*bad_arg = TRUE;
  }
}

void par_color(argc, argv, option, opt_color, bad_arg)
int argc;			/* Number of arguments for this option. */
char *argv[];			/* Arguments for this option. */
int *option;			/* Color flag to set. */
int *opt_color;			/* Color to set. */
int *bad_arg;			/* Set to true if an error is detected. */
{
/* Parse a -foreground or -background specification. */

  if (argc != 1 || *option) *bad_arg = TRUE;
  *option = TRUE;
  if (argc == 1) {
	if (strcmp(argv[0], "black") == 0)
		*opt_color = BLACK;
	else if (strcmp(argv[0], "red") == 0)
		*opt_color = RED;
	else if (strcmp(argv[0], "green") == 0)
		*opt_color = GREEN;
	else if (strcmp(argv[0], "yellow") == 0)
		*opt_color = YELLOW;
	else if (strcmp(argv[0], "blue") == 0)
		*opt_color = BLUE;
	else if (strcmp(argv[0], "magenta") == 0)
		*opt_color = MAGENTA;
	else if (strcmp(argv[0], "cyan") == 0)
		*opt_color = CYAN;
	else if (strcmp(argv[0], "white") == 0)
		*opt_color = WHITE;
	else if (strcmp(argv[0], "default") == 0)
		*opt_color = DEFAULT;
	else if (isdigit(argv[0][0]))
	    *opt_color = atoi(argv[0]);
	else    
		*bad_arg = TRUE;
  }
}

void parse_clear(argc, argv, option, opt_all, bad_arg)
int argc;			/* Number of arguments for this option. */
char *argv[];			/* Arguments for this option. */
int *option;			/* Clear flag to set. */
int *opt_all;			/* Clear all switch to set or reset. */
int *bad_arg;			/* Set to true if an error is detected. */
{
/* Parse a -clear specification. */

  if (argc > 1 || *option) *bad_arg = TRUE;
  *option = TRUE;
  if (argc == 1) {
	if (strcmp(argv[0], "all") == 0)
		*opt_all = TRUE;
	else if (strcmp(argv[0], "rest") == 0)
		*opt_all = FALSE;
	else
		*bad_arg = TRUE;
  } else {
	*opt_all = TRUE;
  }
}

void parse_blank(argc, argv, option, opt_all, bad_arg)
int argc;			/* Number of arguments for this option. */
char *argv[];			/* Arguments for this option. */
int *option;			/* Clear flag to set. */
int *opt_all;			/* Clear all switch to set or reset. */
int *bad_arg;			/* Set to true if an error is detected. */
{
/* Parse a -clear specification. */

  if (argc > 1 || *option) *bad_arg = TRUE;
  *option = TRUE;
  if (argc == 1) {
	*opt_all = atoi(argv[0]);
	if ((*opt_all > 60) || (*opt_all < 0))
		*bad_arg = TRUE;
  } else {
	*opt_all = 0;
  }
}

#define STRCMP(str1,str2) strncmp(str1,str2,3)

void parse_option(option, argc, argv, bad_arg)
char *option;			/* Option with leading '-' removed. */
int argc;			/* Number of arguments for this option. */
char *argv[];			/* Arguments for this option. */
int *bad_arg;			/* Set to true if an error is detected. */
{
/* Parse a single specification. */

  if (STRCMP(option, "term") == 0)
	parse_term(argc, argv, &opt_term, &opt_te_terminal_name, bad_arg);
  else if (STRCMP(option, "reset") == 0)
	parse_none(argc, argv, &opt_reset, bad_arg);
  else if (STRCMP(option, "initialize") == 0)
	parse_none(argc, argv, &opt_initialize, bad_arg);
  else if (STRCMP(option, "cursor") == 0)
	parse_switch(argc, argv, &opt_cursor, &opt_cu_on, bad_arg);
  else if (STRCMP(option, "keyboard") == 0)
	parse_keyboard(argc, argv, &opt_keyboard, &opt_ke_type, bad_arg);
  else if (STRCMP(option, "linewrap") == 0)
	parse_switch(argc, argv, &opt_linewrap, &opt_li_on, bad_arg);
  else if (STRCMP(option, "snow") == 0)
	parse_switch(argc, argv, &opt_snow, &opt_sn_on, bad_arg);
  else if (STRCMP(option, "softscroll") == 0)
	parse_switch(argc, argv, &opt_softscroll, &opt_so_on, bad_arg);
  else if (STRCMP(option, "default") == 0)
	parse_none(argc, argv, &opt_default, bad_arg);
  else if (STRCMP(option, "foreground") == 0)
	par_color(argc, argv, &opt_foreground, &opt_fo_color, bad_arg);
  else if (STRCMP(option, "background") == 0)
	par_color(argc, argv, &opt_background, &opt_ba_color, bad_arg);
  else if (STRCMP(option, "bold") == 0)
	parse_switch(argc, argv, &opt_bold, &opt_bo_on, bad_arg);
  else if (STRCMP(option, "blink") == 0)
	parse_switch(argc, argv, &opt_blink, &opt_bl_on, bad_arg);
  else if (STRCMP(option, "reverse") == 0)
	parse_switch(argc, argv, &opt_reverse, &opt_re_on, bad_arg);
  else if (STRCMP(option, "underline") == 0)
	parse_switch(argc, argv, &opt_underline, &opt_un_on, bad_arg);
  else if (STRCMP(option, "store") == 0)
	parse_none(argc, argv, &opt_store, bad_arg);
  else if (STRCMP(option, "clear") == 0)
	parse_clear(argc, argv, &opt_clear, &opt_cl_all, bad_arg);
  else if (STRCMP(option, "blank") == 0)
	parse_blank(argc, argv, &opt_blank, &opt_bl_min, bad_arg);
  else
	*bad_arg = TRUE;
}

/* End of command line parsing routines. */

void usage(prog_name)
char *prog_name;		/* Name of this program. */
{
/* Print error message about arguments, and the command's syntax. */

  fprintf(stderr, "%s: Argument error, usage\n", prog_name);
  fprintf(stderr, "\n");
  fprintf(stderr, "%s\n", prog_name);
  fprintf(stderr, "  [ -term terminal_name ]\n");
  fprintf(stderr, "  [ -reset ]\n");
  fprintf(stderr, "  [ -initialize ]\n");
/*  fprintf(stderr, "  [ -cursor [on|off] ]\n");
  fprintf(stderr, "  [ -snow [on|off] ]\n");
  fprintf(stderr, "  [ -softscroll [on|off] ]\n");
  fprintf(stderr, "  [ -keyboard pc|olivetti|dutch|extended ]\n"); */
  fprintf(stderr, "  [ -linewrap [on|off] ]\n");
  fprintf(stderr, "  [ -default ]\n");
  fprintf(stderr, "  [ -foreground black|blue|green|cyan|red|magenta");
	fprintf(stderr, "|yellow|white|default ]\n");
  fprintf(stderr, "  [ -background black|blue|green|cyan|red|magenta");
	fprintf(stderr, "|yellow|white|default ]\n");
  fprintf(stderr, "  [ -bold [on|off] ]\n");
  fprintf(stderr, "  [ -blink [on|off] ]\n");
  fprintf(stderr, "  [ -reverse [on|off] ]\n");
  fprintf(stderr, "  [ -underline [on|off] ]\n");
  fprintf(stderr, "  [ -store ]\n");
  fprintf(stderr, "  [ -clear [all|rest] ]\n");
  fprintf(stderr, "  [ -blank [0-60] ]\n");
}

char tc_ent_buf[TC_ENT_SIZE];	/* Buffer for storing a termcap entry. */

char *tc_entry(name)
char *name;			/* Termcap capability string to lookup. */
{
/* Return the specified termcap string, or an empty string if no such termcap
 * capability exists.
 */

  char *buf_ptr;

  buf_ptr = tc_ent_buf;
  if (tgetstr(name, &buf_ptr) == NULL) tc_ent_buf[0] = '\0';
  return tc_ent_buf;
}

void perform_sequence(vcterm)
int vcterm;			/* Set if terminal is a virtual console. */
{
/* Perform the selected options. */

  /* -reset. */
  if (opt_reset) {
	printf("%s", tc_entry("rs"));
  }

  /* -initialize. */
  if (opt_initialize) {
	printf("%s", tc_entry("is"));
  }

  /* -cursor [on|off]. */
  if (opt_cursor) {
	if (opt_cu_on)
		printf("%s", tc_entry("ve"));
	else
		printf("%s", tc_entry("vi"));
  }

  /* -keyboard pc|olivetti|dutch|extended.  Vc only. */
  /*if (opt_keyboard && vcterm) {
	switch (opt_ke_type) {
	    case PC:
		printf("%s%s%s", DCS, "keyboard.pc", ST);
		break;
	    case OLIVETTI:
		printf("%s%s%s", DCS, "keyboard.olivetti", ST);
		break;
	    case DUTCH:
		printf("%s%s%s", DCS, "keyboard.dutch", ST);
		break;
	    case EXTENDED:
		printf("%s%s%s", DCS, "keyboard.extended", ST);
		break;
	}
  } */

  /* -linewrap [on|off].  Vc only. */
  if (opt_linewrap && vcterm) {
	if (opt_li_on)
		printf("%sL", DCS); /*, "linewrap.on", ST);*/
	else
		printf("%sl", DCS); /* "linewrap.off", ST);*/
  }

  /* -snow [on|off].  Vc only. */
/*  if (opt_snow && vcterm) {
	if (opt_sn_on)
		printf("%s%s%s", DCS, "snow.on", ST);
	else
		printf("%s%s%s", DCS, "snow.off", ST);
  }*/

  /* -softscroll [on|off].  Vc only. */
  /*if (opt_softscroll && vcterm) {
	if (opt_so_on)
		printf("%s%s%s", DCS, "softscroll.on", ST);
	else
		printf("%s%s%s", DCS, "softscroll.off", ST);
  }*/

  /* -default.  Vc sets default rendition, otherwise clears all
   * attributes.
   */
  if (opt_default) {
	if (vcterm)
		printf("%s%s", ESC, "[0m");
	else
		printf("%s", tc_entry("me"));
  }

  /* -foreground black|red|green|yellow|blue|magenta|cyan|white|default.
   * Vc only.
   */
  if (opt_foreground && vcterm) {
	printf("%s%s%c%s", ESC, "[3", '0' + opt_fo_color, "m");
  }

  /* -background black|red|green|yellow|blue|magenta|cyan|white|default.
   * Vc only.
   */
  if (opt_background && vcterm) {
	printf("%s%s%c%s", ESC, "[4", '0' + opt_ba_color, "m");
  }

  /* -bold [on|off].  Vc behaves as expected, otherwise off turns off
   * all attributes.
   */
  if (opt_bold) {
	if (opt_bo_on)
		printf("%s", tc_entry("md"));
	else {
		if (vcterm)
			printf("%s%s", ESC, "[22m");
		else
			printf("%s", tc_entry("me"));
	}
  }

  /* -blink [on|off].  Vc behaves as expected, otherwise off turns off
   * all attributes.
   */
  if (opt_blink) {
	if (opt_bl_on)
		printf("%s", tc_entry("mb"));
	else {
		if (vcterm)
			printf("%s%s", ESC, "[25m");
		else
			printf("%s", tc_entry("me"));
	}
  }

  /* -reverse [on|off].  Vc behaves as expected, otherwise off turns
   * off all attributes.
   */
  if (opt_reverse) {
	if (opt_re_on)
		printf("%s", tc_entry("mr"));
	else {
		if (vcterm)
			printf("%s%s", ESC, "[27m");
		else
			printf("%s", tc_entry("me"));
	}
  }

  /* -underline [on|off]. */
  if (opt_underline) {
	if (opt_un_on)
		printf("%s", tc_entry("us"));
	else
		printf("%s", tc_entry("ue"));
  }

  /* -store.  Vc only. */
  if (opt_store && vcterm) {
/*	printf("%s%s%s", DCS, "rendition.set", ST); */
	printf("%sS", DCS);
  }

  /* -clear [all|rest]. */
  if (opt_clear) {
	if (opt_cl_all)
		printf("%s", tc_entry("cl"));
	else
		printf("%s", tc_entry("cd"));
  }

  /* -blank [0-60]. */
  if (opt_blank) 
    printf("%s[%d;%d;%dl", ESC, opt_bl_min, opt_bl_min+13,opt_bl_min+17 );
}

void main(argc, argv)
int argc;
char *argv[];
{
  int bad_arg;			/* Set if error in arguments. */
  int arg, modifier;
  char *term;			/* Terminal type. */
  int vcterm;			/* Set if terminal is a virtual console. */

  int bag_arg = FALSE;

  if (argc < 2) bad_arg = TRUE;

  /* Parse arguments. */

  for (arg = 1; arg < argc;) {
	if (*argv[arg] == '-') {

		/* Parse a single option. */

		for (modifier = arg + 1; modifier < argc; modifier++) {
			if (*argv[modifier] == '-') break;
		}
		parse_option(argv[arg] + 1, modifier - arg - 1,
			     &argv[arg + 1], &bad_arg);
		arg = modifier;
	} else {

		bad_arg = TRUE;
		arg++;
	}
  }

  /* Display syntax message if error in arguments. */

  if (bad_arg) {
	usage(argv[0]);
	exit(1);
  }

  /* Find out terminal name. */

  if (opt_term) {
	term = opt_te_terminal_name;
  } else {
	term = getenv("TERM");
	if (term == NULL) {
		fprintf(stderr, "%s: $TERM is not defined.\n", argv[0]);
		exit(1);
	}
  }

  /* Find termcap entry. */

  if (tgetent(tc_buf, term) != 1) {
	fprintf(stderr, "%s: Could not find termcap entry for %s.\n",
		argv[0], term);
	exit(1);
  }

  /* See if the terminal is a virtual console terminal. */

  vcterm = (strcmp(term, VCTERM) == 0) || (strcmp(term, VCTERM_AM) == 0);

  /* Perform the selected options. */

  perform_sequence(vcterm);

  exit(0);
}
