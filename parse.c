#include "globals.h"

#define PRINT_LINENUM_EXIT() \
	if (linenum) printf(" on line %d",linenum); \
	puts("."); \
	exit(1)


void parseTerminalSize(char *str, int linenum)
{
	char *ptr = strchr(str,'x');

	if (ptr && ptr > str)
	{
		term_width = atoi(str);
		term_height = atoi(ptr+1);
		if (term_width > 0 && term_height > 0) 
		{
			if (term_width < MIN_TERM_WIDTH || 
			    term_height < MIN_TERM_HEIGHT)
			{
				if (linenum)
				{
					errprintf("Terminal size too small on line %d. Minimum is %dx%d.\n",
						linenum,
						MIN_TERM_WIDTH,MIN_TERM_HEIGHT);
				}
				else
				{
					errprintf("Terminal size too small. Minimum is %dx%d.\n",
						MIN_TERM_WIDTH,MIN_TERM_HEIGHT);
				}
				exit(1);
			}
			flags.fixed_term_size = 1;
			return;
		}
	}
	errprintf("Terminal size format must be <width>x<height>");
	PRINT_LINENUM_EXIT();
}




void parseTerminalPane(char *pane, int linenum)
{
	if (!strcmp(pane,"hex")) term_pane = PANE_HEX;
	else if (!strcmp(pane,"text")) term_pane = PANE_TEXT;
	else if (!strcmp(pane,"cmd")) term_pane = PANE_CMD;
	else
	{
		errprintf("Unknown pane \"%s\"",pane);
		PRINT_LINENUM_EXIT();
	}
}




void parseCursorType(char *type, int linenum)
{
	if (!strcmp(type,"blk")) setCursorType(CUR_BLOCK);
	else if (!strcmp(type,"udl")) setCursorType(CUR_UNDERLINE);
	else if (!strcmp(type,"bar")) setCursorType(CUR_HALF_BLOCK);
	else
	{
		errprintf("Unknown cursor type \"%s\"",type);
		PRINT_LINENUM_EXIT();
	}
}




void parseSubChar(char *val, int linenum)
{
	if (strlen(val) == 1 && IS_PRINTABLE(val[0]))
	{
		substitute_char = val[0];
		return;
	}
	if (!strcmp(val,"space")) 
	{
		substitute_char = ' ';
		return;
	}
	errprintf("The substitute must be 1 character and printable or the word \"space\"");
	PRINT_LINENUM_EXIT();
}
