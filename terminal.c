#include "globals.h"

#define DEF_TERM_WIDTH  80
#define DEF_TERM_HEIGHT 24


void parseTerminalSize(char *str)
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
				errprintf("Terminal size too small. Minimum is %dx%d\n",
					MIN_TERM_WIDTH,MIN_TERM_HEIGHT);
				exit(1);
			}
			flags.fixed_term_size = 1;
			return;
		}
	}
	errprintf("Terminal size format is <width>x<height>.\n");
	exit(1);
}




void getTermType()
{
	char *term = getenv("TERM");

	/* Default to xterm */
	term_type = TERM_XTERM;

	if (!strcmp(term,"linux")) term_type = TERM_LINUX_CONSOLE; 
	else if (!strncmp(term,"vt",2)) term_type = TERM_VT;
}




void getTermSize()
{
	struct winsize ws;

	if (!flags.fixed_term_size) 
	{
		if (ioctl(1,TIOCGWINSZ,&ws) == -1)
		{
			syserrprintf("ioctl");
			term_width = DEF_TERM_WIDTH;
			term_height = DEF_TERM_HEIGHT;
		}
		else
		{
			/* It seems with dumb terminals ioctl() can return 
			   success but set the dimensions to zero */
			term_width = ws.ws_col ? ws.ws_col : DEF_TERM_WIDTH;
			term_height = ws.ws_row ? ws.ws_row : DEF_TERM_HEIGHT;
		}
	}

	term_pane_cols = (int)((float)term_width * 0.72);
	if (term_pane_cols % 3) term_pane_cols -= (term_pane_cols % 3);
	term_pane_cols /= 3;

	/* Set up draw params */
	if (term_height > 3 && term_width > 9)
	{
		term_div_y = term_height - CMD_PANE_HEIGHT - 1;
		term_textbox_y = term_div_y + 2;
	}
	else
	{
		term_div_y = 3;
		term_textbox_y = 5;
	}
	cmd_y = term_div_y + 1;
}




void clearScreen()
{
	/* [H makes the cursor go to home, ie 1,1 (or 0,0 using our co-ords) */
	write(STDOUT,"\033[2J\033[H",7);
}




void clearLine(int y)
{
	locate(0,y);
	write(STDOUT,"\033[0K",4);
}




/*** Starts at 0,0. Terminal starts at 1,1 so add 1 ***/
void locate(int x, int y)
{
	/* If the xterm gets squished up to nothing the co-ords may go
	   negative */
	if (x >= 0 && y >= 0)
	{
		printf("\033[%d;%dH",y+1,x+1);
		fflush(stdout);
	}
}




void setPaneStart(u_char *mem_pos)
{
	u_long diff;

	assert(mem_pos >= mem_start);
	mem_cursor = mem_pos;

	/* Snap to the column width so eg if mem_pos = 3 we actually
	   set the pane start to 0 */
	if (mem_pos)
	{
		diff = (u_long)(mem_pos - mem_start) % term_pane_cols;
		if (diff) mem_pos -= diff;
		assert(mem_pos >= mem_start);
	}
	mem_pane_start = mem_pos;
	drawScreen();
}




/*** Move cursor to point on screen defined by mem_cursor ***/
void positionCursor()
{
	int dist;
	int cx;
	int cy;
	int x;

	/* Update file position in banner */
	if (!drawBanner(BAN_LINE_F_F)) return;

	if (term_pane == PANE_CMD)
	{
		locate(cmd_x,cmd_y);
		return;
	}
	dist = (int)(mem_cursor - mem_pane_start);
	x = dist % term_pane_cols;
	if (term_pane == PANE_HEX)
		cx = x * 3 + flags.cur_hex_right;
	else 
		cx = term_pane_cols * 3 + x + 2; 
	cy = dist / term_pane_cols + BAN_PANE_HEIGHT + 1;

	locate(cx,cy);
}




void setCursorType(int type)
{
	/* Order is block, underline/half block, bar. They all blink. 
	   VT terminals don't seem to support changing the cursor type */
	const char *code[NUM_TERM_TYPES-1][NUM_CURSOR_TYPES] =
	{
		/* Xterm and MacOS */
		{ "\033[0 q","\033[5 q","\033[3 q" },

		/* Linux console */
		{ "\033[?6c","\033[?4c","\033[?2c" }
	};
	assert(type >= 0 && type < NUM_CURSOR_TYPES);

	if (term_type != TERM_VT)
	{
		write(STDOUT,code[term_type][type],5);
		cursor_type = type;
	}
}




void scrollUp()
{
	mem_pane_start -= term_pane_cols;
	if (mem_pane_start < mem_start) mem_pane_start = mem_start;
	mem_decode_view = NULL;
	drawScreen();
}




void scrollDown()
{
	mem_pane_start += term_pane_cols;
	if (mem_pane_start > mem_end) mem_pane_start = mem_end;
	mem_decode_view = NULL;
	drawScreen();
}




void pageUp()
{
	int cnt;
	int offset;

	if (mem_pane_start == mem_start) return;

	/* Get max number of bytes to page */
 	cnt = term_pane_cols * (term_div_y - BAN_PANE_HEIGHT - 2);

	/* Shift start of pane back */
	if ((mem_pane_start -= cnt) < mem_start) mem_pane_start = mem_start;

	/* Get cursor offset from right hand side */
	offset = (int)(mem_pane_end - mem_cursor) % term_pane_cols;

	/* Call first as it sets mem_pane_end */
	mem_decode_view = NULL;
	drawScreen();
	if (mem_cursor > mem_pane_end)
	{
		mem_cursor = mem_pane_end - offset;
		assert(mem_cursor >= mem_start);
		positionCursor();
	}
}




void pageDown()
{
	int cnt;
	int offset;

	if (mem_pane_end == mem_end) return;

	cnt = term_pane_cols * (term_div_y - BAN_PANE_HEIGHT - 2);
	offset = (int)(mem_cursor - mem_pane_start) % term_pane_cols;

	if ((mem_pane_start += cnt) > mem_end) mem_pane_start = mem_end;
	if (mem_cursor < mem_pane_start)
	{
		mem_cursor = mem_pane_start + offset;
		assert(mem_cursor <= mem_end);
	}
	mem_decode_view = NULL;
	drawScreen();
}
