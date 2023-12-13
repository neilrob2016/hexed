#include "globals.h"

#define BACKSPACE 8
#define TAB       9
#define ESC       27

void quit();
void switchPane();
void cursorUp();
void cursorDown();
void cursorLeft();
void cursorRight();

void runCommand(u_char c);
void stateCmd(u_char c);
void stateText(u_char c);
void stateYN(u_char c);
void findText();
void setCommandText(char *str);
void setDecodeView();



void initKeyboard()
{
	struct termios tio;

	if (tcgetattr(STDIN,&tio) == -1)
	{
		syserrprintf("tcgetattr");
		exit(1);
	}
	saved_tio = tio;

	/* Echo off, canonical off */
	tio.c_lflag &= ~ECHO;
	tio.c_lflag &= ~ICANON;

	/* Min return 1 byte, no delay */
	tio.c_cc[VMIN] = 1;
	tio.c_cc[VTIME] = 0;

	/* Set new state */
	if (tcsetattr(STDIN,TCSANOW,&tio) == -1)
	{
		syserrprintf("tcsetattr");
		doExit(1);
	}
}




void readKeyboard()
{
	/* Keyboard escape sequences */
	enum
	{
		/* 0 */
		ESC_K,
		ESC_J,
		ESC_UP_ARROW,
		ESC_DOWN_ARROW,
		ESC_LEFT_ARROW,
	
		/* 5 */
		ESC_RIGHT_ARROW,
		ESC_INSERT,
		ESC_DELETE,
		ESC_PAGE_UP,
	
		/* 10 */
		ESC_PAGE_DOWN,
		ESC_CON_F1,
		ESC_CON_F2,
		ESC_CON_F3,
		ESC_CON_F4,
		
		/* 15 */
		ESC_CON_F5,
		ESC_CON_F6,
		ESC_CON_F7,
		ESC_CON_F8,
		ESC_TERM_F1,
	
		/* 20 */
		ESC_TERM_F2,
		ESC_TERM_F3,
		ESC_TERM_F4,
		ESC_TERM_F5,
		ESC_TERM_F6,

		/* 25 */
		ESC_TERM_F7,
		ESC_TERM_F8,
		ESC_SHIFT_HOME,
		ESC_SHIFT_END,
	
		NUM_ESC_SEQS
	};
	
	char *esc_seq[NUM_ESC_SEQS] =
	{
		/* 0 */
		"k",
		"j",
		"[A",
		"[B",
		"[D",
	
		/* 5 */
		"[C",
		"[2~",
		"[3~",
		"[5~",
	
		/* 10 */
		"[6~",
		"[[A",
		"[[B",
		"[[C",
		"[[D",
	
		/* 15 */
		"[[E",
		"[[F",
		"[[G",
		"[[H",
		"OP",
	
		/* 20 */
		"OQ",
		"OR",
		"OS",
		"[15~",
		"[17~",

		/* 25 */
		"[18~",
		"[19~",
		"[H",
		"[F"
	};
	time_t now;
	char s[6];
	int len;
	int i;

	/* Escape codes will be returned in one go */
	switch((len = read(STDIN,s,sizeof(s)-1)))
	{
	case -1:
		syserrprintf("read");
		return;
	case 0:
		errprintf("Stdin closed!\n");
		doExit(1);
	}
	switch(s[0])
	{
	case TAB:
		switchPane();
		break;
	case ESC:
		switch(len)
		{
		case 1:
			/* If escape pressed twice within 1 sec then exit */
			now = time(0);
			if (esc_time && !(now - esc_time)) quit();
			esc_time = now;
			break;
		case 3:
		case 4:
		case 5:
			break;
		default:
			return;
		}
		s[len] = 0;
		for(i=0;i < NUM_ESC_SEQS;++i)
		{
			if (strcmp(s+1,esc_seq[i])) continue;

			switch(i)
			{
			case ESC_UP_ARROW:
				cursorUp();
				break;
			case ESC_DOWN_ARROW:
				cursorDown();
				break;
			case ESC_LEFT_ARROW:
				cursorLeft();
				break;
			case ESC_RIGHT_ARROW:
				cursorRight();
				break;

			case ESC_CON_F1:
			case ESC_TERM_F1:
			case ESC_PAGE_UP:
				pageUp();
				break;

			case ESC_CON_F2:
			case ESC_TERM_F2:
			case ESC_PAGE_DOWN:
				pageDown();
				break;

			case ESC_CON_F3:
			case ESC_TERM_F3:
			case ESC_INSERT:
				flags.insert_mode = !flags.insert_mode;
				drawBanner(BAN_LINE3);
				positionCursor(0);
				break;

			case ESC_CON_F4:
			case ESC_TERM_F4:
			case ESC_DELETE:
				if (term_pane != PANE_CMD) deleteAtCursorPos(1);
				break;

			case ESC_CON_F5:
			case ESC_TERM_F5:
				runCommand('U');
				break;

			case ESC_CON_F6:
			case ESC_TERM_F6:
				/* If search text set then look for next else
				   ask for new search */
				runCommand(search_text_len ? 'N' : 'T');
				break;

			case ESC_CON_F7:
			case ESC_TERM_F7:
				runCommand('D');
				break;

			case ESC_TERM_F8:
				setCursorType((cursor_type + 1) % NUM_CURSOR_TYPES);
				drawBanner(BAN_LINE4);
				positionCursor(0);
				break;

			case ESC_SHIFT_HOME:
				if (term_pane != PANE_CMD)
					setPaneStart(mem_start);
				break;

			case ESC_SHIFT_END:
				if (term_pane != PANE_CMD)
					setPaneStart(mem_end);
				break;

			default:
				assert(0);
			}
		}
		break;
	default:
		if (term_pane == PANE_CMD)
			runCommand(s[0]);
		else
			changeFileData(s[0]);
	}
}




/****************************** Key functions ******************************/

void quit()
{
	locate(0,term_textbox_y+2);
	colprintf("~BR~FW*** EXIT ***\n");
	doExit(0);
}



void switchPane()
{
	term_pane = (term_pane + 1) % NUM_PANES;
	drawBanner(BAN_LINE3);
	positionCursor(0);
}




void cursorUp()
{
	/* If we're already on the top line don't go to the very start */
	if (mem_cursor - mem_start >= term_pane_cols)
	{
		mem_cursor -= term_pane_cols;
		if (mem_cursor < mem_start) mem_cursor = mem_start;

		if (mem_cursor < mem_pane_start) scrollUp();
		positionCursor(1);
	}
}




void cursorDown()
{
	mem_cursor += term_pane_cols;
	if (mem_cursor > mem_end) mem_cursor = mem_end;
	if (mem_cursor > mem_pane_end) scrollDown();
	positionCursor(1);
}




void cursorLeft()
{
	if (term_pane == PANE_HEX && flags.cur_hex_right)
	{
		flags.cur_hex_right = 0;
		positionCursor(1);
	}
	else if (mem_cursor > mem_start)
	{
		flags.cur_hex_right = 1;
		--mem_cursor;
		if (mem_cursor < mem_pane_start)
		{
			mem_pane_start = mem_cursor;
			drawScreen();
		}
		else positionCursor(1);
	}
}




void cursorRight()
{
	if (term_pane == PANE_HEX && !flags.cur_hex_right)
	{
		flags.cur_hex_right = 1;
		positionCursor(1);
	}
	else if (mem_cursor < mem_end)
	{
		flags.cur_hex_right = 0;
		++mem_cursor;
		positionCursor(1);
		if (mem_cursor > mem_pane_end)
		{
			scrollDown();
			positionCursor(1);
		}
	}
}


/***************************** Command functions *****************************/

void runCommand(u_char c)
{
	LOOP:
	switch(cmd_state)
	{
	case STATE_CMD:
		stateCmd(c);
		break;
	case STATE_TEXT:
		stateText(c);
		break;
	case STATE_YN:
		stateYN(c);
		break;
	default:
		resetCommand();
		goto LOOP;
	}
	drawCmdPane();
	positionCursor(0);
}




/*** User has entered a command character. Some commands are executed
     immediately, others need to switch to STATE_TEXT for input. ***/
void stateCmd(u_char c)
{
	if (!isalpha(c)) 
	{
		if (c == '\n') resetCommand();
		return;
	}

	resetCommand();
	user_cmd = toupper(c);

	switch(user_cmd)
	{
	case 'A':
		setCommandText(filename);
		cmd_state = STATE_YN;
		break;
	case 'C':
		flags.use_colour = !flags.use_colour;
		clearScreen();
		drawScreen();
		break;
	case 'D':
		setDecodeView();
		break;
	case 'F':
		break;
	case 'H':
		help_page = !help_page;
		/* Gets reset at the bottom so return here */
		return;
	case 'I':
		/* Might not be in command pane if F6 used */
		if (term_pane != PANE_CMD) term_pane = PANE_CMD;
		search_text_len = 0;
		flags.search_ign_case = 1;
		flags.search_hex = 0;
		cmd_state = STATE_TEXT;
		break;
	case 'N':
		findText();
		break;
	case 'Q':
		cmd_state = STATE_YN;
		break;
	case 'R':
		clearScreen();
		drawScreen();
		break;
	case 'G':
	case 'S':
		cmd_state = STATE_TEXT;
		break;
	case 'T':
		if (term_pane != PANE_CMD) term_pane = PANE_CMD;
		search_text_len = 0;
		flags.search_ign_case = 0;
		flags.search_hex = 0;
		cmd_state = STATE_TEXT;
		break;
	case 'U':
		undo();
		drawScreen();
		break;
	case 'X':
		flags.search_ign_case = 0;
		cmd_state = STATE_TEXT;
		break;
	case 'V':
		break;
	case 'Y':
	case 'Z':
		sr_count = 0;
		cmd_state = STATE_TEXT;
		drawBanner(BAN_LINE4);
		sr_state = (user_cmd == 'Z' ? SR_STATE_HEX1 : SR_STATE_TEXT1);
		break;
	default:
		cmd_state = STATE_ERR_CMD;
		break;
	}
	help_page = 0;
}




/*** User is entering text for command ***/
void stateText(u_char c)
{
	if (c == '\n') 
	{
		/* User pressed enter, do something Muttley! */
		switch(user_cmd)
		{
		case 'S':
			if (cmd_text_len)
				cmd_state = STATE_YN;
			else
				resetCommand();
			break;
		case 'I':
		case 'T':
		case 'X':
			findText();
			break;
		case 'G':
			if (cmd_text_len)
			{
				goto_pos = atol(cmd_text);
				resetCommand();

				if (goto_pos >= file_size)
					cmd_state = STATE_ERR_INPUT;
				else
				{
					setPaneStart(mem_start + goto_pos);
					cmd_state = STATE_CMD;
				}
			}
			else resetCommand();
			break;
		default:
			if (sr_state != SR_STATE_NONE)
				doSearchAndReplace();
			else
				assert(0);
		}
		return;
	}
	if (cmd_text_len > CMD_TEXT_SIZE) return;

	/* User entering text */
	if (c == BACKSPACE || c == ASCII_DEL)
	{
		/* Delete last char */
		if (cmd_text_len)
		{
			--cmd_text_len;
			cmd_text[cmd_text_len] = 0;
		}
		return;
	}

	/* Add char to cmd_text */
	switch(user_cmd)
	{
	case 'I':
	case 'T':
	case 'Y':
		cmd_text[cmd_text_len++] = (char)c;
		break;
	case 'G':
		if (isdigit(c)) cmd_text[cmd_text_len++] = (char)c;
		break;
	case 'S':
		if (!isspace(c)) cmd_text[cmd_text_len++] = (char)c;
		break;
	case 'X':
	case 'Z':
		c = toupper(c);
		if (isdigit(c) || (c >= 'A' && c <= 'F'))
			cmd_text[cmd_text_len++] = c;
		break;
	default:
		/* In HEX2 and TEXT2 search and replace states user_cmd has 
		   been reset as we can't use it to differentiate between 
		   states 1 & 2 */
		switch(sr_state)
		{
		case SR_STATE_HEX2:
			c = toupper(c);
			if (isdigit(c) || (c >= 'A' && c <= 'F'))
				cmd_text[cmd_text_len++] = c;
			break;

		case SR_STATE_TEXT2:
			cmd_text[cmd_text_len++] = (char)c;
			break;

		default:
			assert(0);
		}
	}
	cmd_text[cmd_text_len] = 0;
}




void stateYN(u_char c)
{
	c = toupper(c);
	if (c == 'Y')
	{
		switch(user_cmd)
		{
		case 'A':
		case 'S':
			if (saveFile(cmd_text))
			{
				resetCommand();
				cmd_state = STATE_SAVE_OK;
			}
			else
			{
				resetCommand();
				cmd_state = STATE_ERR_SAVE;
			}
			break;
		case 'Q':
			quit();
		default:
			assert(0);
		}
	}
	else if (c == 'N') resetCommand();
}




void setCommandText(char *str)
{
	cmd_text_len = strlen(str);
	if (cmd_text_len > CMD_TEXT_SIZE)
		cmd_text_len = CMD_TEXT_SIZE;
	strncpy(cmd_text,str,cmd_text_len);
	cmd_text[cmd_text_len] = 0;
}




void setDecodeView()
{
	if ((u_long)(mem_end - mem_cursor) < 1)
	{
		cmd_state = STATE_ERR_DATA_VIEW;
		mem_decode_view = NULL;
	}
	else mem_decode_view = mem_cursor;
	decode_page = !decode_page;

	drawScreen();
}
