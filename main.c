/*****************************************************************************
 HEXED

 A colour hex editor for Linux and MacOS.

 Written 2022
 *****************************************************************************/
#define MAINFILE
#include "globals.h"

void init();
void parseCmdLine(int argc, char **argv);
void versionExit();
void mainloop();


int main(int argc, char **argv)
{
	parseCmdLine(argc,argv);
	init();
	mapFile();
	getTermSize();
	mainloop();
	return 0;
}




void init()
{
	mem_start = NULL;
	mem_end = NULL;
	mem_cursor = NULL;
	mem_pane_start = NULL;
	mem_pane_end = NULL;
	mem_search_find_start = NULL;
	mem_search_find_end = NULL;
	mem_decode_view = NULL;

	help_page = 0;
	esc_time = 0;
	total_updates = 0;
	total_inserts = 0;
	total_deletes = 0;
	total_undos = 0;

	resetCommand();
	initKeyboard();
	initSignals();
	initUndo();
}




void parseCmdLine(int argc, char **argv)
{
	char *val;
	char c;
	int len;
	int i;

	if (argc < 2) goto USAGE;


	filename = NULL;
	term_pane = PANE_CMD;
	insert_char = INSERT_CHAR; 
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
		case 'n':
			flags.use_colour = 0;
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
			if (!strcmp(val,"blk"))
				setCursorType(CUR_BLOCK);
			else if (!strcmp(val,"udl"))
				setCursorType(CUR_UNDERLINE);
			else if (!strcmp(val,"bar"))
				setCursorType(CUR_HALF_BLOCK_BAR);
			else goto USAGE;
			break;
		case 'i':
			len = strlen(val);
			if (len == 1) insert_char = (u_char)val[0];
			else if (len == 2)
			{
				insert_char = strtol(val,NULL,16);
				if (errno == EINVAL) goto USAGE;
			}
			else goto USAGE;
			break;
		case 'p':
			if (!strcmp(val,"hex")) term_pane = PANE_HEX;
			else if (!strcmp(val,"text")) term_pane = PANE_TEXT;
			else if (!strcmp(val,"cmd")) term_pane = PANE_CMD;
			else goto USAGE;
			break;
		case 's':
			if (strlen(val) == 1 && IS_PRINTABLE(val[0]))
			{
				substitute_char = val[0];
				break;
			}
			goto USAGE;
		case 'x':
			parseTerminalSize(val);
			break;
		default:
			goto USAGE;
		}
	}
	if (filename) return;

	USAGE:
	printf("Usage: %s\n"
	       "       -f <filename>\n"
	       "       -i <insert char/hex> : One character is treated as ascii, two must be\n"
	       "                              a valid hex value. Eg: X or 20.\n"
	       "                              Default = '%c' (0x%02X)\n"
	       "       -s <substitute char> : Substitute character in text pane for unprintable\n"
	       "                              characters. Must be printable itself.\n"
	       "                              Default = '%c'\n"
	       "       -c <cursor type>     : Options are 'blk','udl' and 'bar'\n"
	       "                              Default = 'blk' (block)\n"
	       "       -p <start pane>      : Options are 'hex','text' or 'cmd'.\n"
	       "                              Default = 'cmd'\n"
	       "       -x <width>x<height>  : Force terminal size. Eg: '80x25'\n"
	       "       -n                   : No colour\n"
	       "       -v                   : Print version and build date then exit\n"
	       "Note: All arguments are optional except -f\n",
		argv[0],INSERT_CHAR,INSERT_CHAR,SUBSTITUTE_CHAR);
	exit(1);
}




void versionExit()
{
	putchar('\n');
	version();
	puts("\n");
	exit(0);
}




void mainloop()
{
	fd_set mask;

	drawScreen();

	while(1)
	{
		FD_ZERO(&mask);
		FD_SET(STDIN,&mask);
		switch(select(FD_SETSIZE,&mask,NULL,NULL,NULL))
		{
		case -1:
			if (errno == EINTR)
			{
				/* Can only be SIGWINCH here so redraw */
				getTermSize();
				drawScreen();
				continue;
			}
			syserrprintf("select");
			doExit(1);
		case 0:
			assert(0);
		}
		readKeyboard();
	}
}
