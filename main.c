/*****************************************************************************
 HEXED

 A colour hex editor for Linux and MacOS.

 Initial version written in 2022
 *****************************************************************************/
#define MAINFILE
#include "globals.h"

void init(void);
void parseCmdLine(int argc, char **argv);
void versionExit(void);
void mainloop(void);


int main(int argc, char **argv)
{
	parseCmdLine(argc,argv);
	parseRCFile();
	init();
	mapFile();
	getTermSize();
	mainloop();
	return 0;
}




void init(void)
{
	mem_start = NULL;
	mem_end = NULL;
	mem_cursor = NULL;
	mem_pane_start = NULL;
	mem_pane_end = NULL;
	mem_search_find_start = NULL;
	mem_search_find_end = NULL;
	mem_decode_view = NULL;

	help_page = HELP_FKEYS;
	decode_page = 0;
	esc_time = 0;
	total_updates = 0;
	total_inserts = 0;
	total_deletes = 0;
	total_undos = 0;
	sr_state = SR_STATE_NONE;
	sr_cnt = 0;

	/* Network byte order is big endian */
	if (htons(0x1234) == 0x1234)
	{
		endian[SYS] = "big   ";
		endian[REV] = "little"; 
	}
	else
	{
		endian[SYS] = "little";
		endian[REV] = "big   ";  
	}

	resetCommand();
	initKeyboard();
	initSignals();
	initUndo();
}




void parseCmdLine(int argc, char **argv)
{
	char *val;
	char c;
	int i;

	filename = NULL;
	rc_filename = NULL;
	term_pane = PANE_CMD;
	substitute_char = SUBSTITUTE_CHAR;

	bzero(&flags,sizeof(flags));
	flags.use_colour = 1;
	/* Need to do this now so setCursorType() works */
	getTermType();

	setCursorType(CUR_BLOCK);

	for(i=1;i < argc;++i)
	{
		if (argv[i][0] != '-') goto USAGE;
		c = argv[i][1];

		switch(c)
		{
		case 'i':
			flags.insert_mode = 1;
			flags.insert_mode_set = 1; /* Override rc setting */
			continue;
		case 'l':
			flags.lowercase_hex = 1;
			flags.lowercase_hex_set = 1;
			continue;
		case 'n':
			flags.use_colour = 0;
			flags.use_colour_set = 1;
			continue;
		case 'u':
			flags.retain_preundo_pos = 1;
			flags.retain_preundo_pos_set = 1;
			continue;
		case 'v':
			versionExit();
		}

		if (++i == argc) goto USAGE;
		val = argv[i];

		switch(c)
		{
		case 'f':
			setFileName(val);
			break;
		case 'c':
			parseCursorType(val,0);
			flags.cursor_set = 1;
			break;
		case 'p':
			parseTerminalPane(val,0);
			flags.pane_set = 1;
			break;
		case 'r':
			rc_filename = argv[i];
			break;
		case 's':
			parseSubChar(val,0);
			flags.subchar_set = 1;
			break;
		case 'x':
			parseTerminalSize(val,0);
			flags.termsize_set = 1;
			break;
		default:
			goto USAGE;
		}
	}
	return;

	USAGE:
	printf("Usage: %s\n"
	       "       -f <filename>\n"
	       "       -r <RC filename>     : Default = ~/%s\n"
	       "       -c <cursor type>     : Options are 'blk','udl' and 'bar'\n"
	       "                              Default = 'blk' (block)\n"
	       "       -p <start pane>      : Options are 'hex','text' or 'cmd'.\n"
	       "                              Default = 'cmd'\n"
	       "       -s <substitute char> : The substitute character in the text pane for\n"
	       "                              unprintable characters. Must be printable itself.\n"
	       "                              Default = '%c'\n"
	       "       -x <width>x<height>  : Force terminal size. Eg: '80x25'\n"
	       "       -i                   : Start in insert mode. Default = overwrite.\n"
	       "       -l                   : Lowercase hex.\n"
	       "       -n                   : No ANSI colour (will still use other ANSI\n"
	       "                              terminal codes)\n"
	       "       -u                   : Keep the cursor in the same relative position in\n"
	       "                              the file after an undo if possible.\n"
	       "       -v                   : Print version and build date then exit\n"
	       "Note:\n"
	       "1) All arguments are optional and they override their equivalent in the RC file.\n"
	       "2) If no filename is given a single byte is allocated and set to zero and the\n"
	       "   mode is set to insert unless its set to overwrite in the RC file.\n",
		argv[0],RC_FILENAME,SUBSTITUTE_CHAR);
	exit(1);
}




void versionExit(void)
{
	putchar('\n');
	version();
	puts("\n");
	exit(0);
}




void mainloop(void)
{
	fd_set mask;

	/* Will never normally see this unless the ANSI clear screen fails
	   for some reason */
	colprintf("~BG*** Initialised ***\n");

	clearScreen();
	drawMain();
	drawCmdPane();

	while(1)
	{
		FD_ZERO(&mask);
		FD_SET(STDIN_FILENO,&mask);
		switch(select(FD_SETSIZE,&mask,NULL,NULL,NULL))
		{
		case -1:
			if (errno == EINTR)
			{
				/* Can only be SIGWINCH here so redraw */
				getTermSize();
				clearScreen();
				drawMain();
				drawCmdPane();
				continue;
			}
			syserrprintf("select");
			doExit(1);
			break; /* Prevents spurious gcc warning */
		case 0:
			assert(0);
		}
		readKeyboard();
	}
}
