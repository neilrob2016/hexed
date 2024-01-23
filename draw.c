#include "globals.h"

#define MAX_UNDO_COL 21
#define DEC_COL2_X   25
#define GET_PRINTABLE(C) (IS_PRINTABLE(C) ? C : substitute_char)

#define PROMPT "Command or TAB ('H' for help): "

void drawDecodeView();
void drawFileInfo();
void drawHelp();
void drawHorizontalLines();


void drawMain()
{
	u_char *line_start;
	u_char *mem_decode_end;
	u_char *ptr;
	u_char printable;
	int len;
	int hex = 1;
	int cnt;
	int y;

	if (!drawBanner(ALL_BAN_LINES)) return;
	drawHorizontalLines();

	if (mem_decode_view)
	{
		len = (int)(mem_end - mem_decode_view + 1);
		if (len < 4) len = 2;
		else if (len < 8) len = 4;
		else len = 8;

		mem_decode_end = mem_decode_view + len - 1;
	}
	else mem_decode_end = NULL;

	cnt = 0;
	for(y=BAN_PANE_HEIGHT+1,ptr=line_start=mem_pane_start;y < term_div_y;)
	{
		if (hex)
		{
			/* Draw hex values */
			if (ptr <= mem_end)
			{
				/* Put decode first as it'll be more transient
				   than a search */
				if (ptr >= mem_decode_view && 
				    ptr <= mem_decode_end)
				{
					if (flags.lowercase_hex)
						colprintf("~BM~FW%02x~RS ",*ptr);
					else
						colprintf("~BM~FW%02X~RS ",*ptr);
				}
				else if (ptr >= mem_search_find_start &&
				         ptr <= mem_search_find_end)
				{
					if (flags.lowercase_hex)
						colprintf("~RV%02x~RS ",*ptr);
					else
						colprintf("~RV%02X~RS ",*ptr);
				}
				else if (flags.lowercase_hex)
					printf("%02x ",*ptr);
				else
					printf("%02X ",*ptr);
				++ptr;
			}
			else printf("   ");

			if (++cnt == term_pane_cols)
			{
				/* Same colour for foreground and background
				   so if solid colours don't work we'll see
				   a dash */
				colprintf("~BT~FT|~RS ");
				hex = 0;
				cnt = 0;
				ptr = line_start;
			}
		}
		else
		{
			/* Draw ascii values */
			if (ptr <= mem_end)
			{
				printable = GET_PRINTABLE(*ptr);

				if (ptr >= mem_decode_view &&
				         ptr <= mem_decode_end)
				{
					colprintf("~FM%c~RS",printable);
				}
				else if (ptr >= mem_search_find_start &&
				         ptr <= mem_search_find_end)
				{
					colprintf("~RV%c~RS",printable);
				}
				else putchar(printable);

				++ptr;
			}
			else putchar(' ');
			if (ptr == mem_search_find_end) colprintf("~RS");

			fflush(stdout);
			if (++cnt == term_pane_cols)
			{
				putchar('\n');
				if (++y == term_div_y) break;
				cnt = 0;
				hex = 1;
				line_start = ptr;
			}
		}
	}
	assert(ptr > mem_pane_start);
	mem_pane_end = ptr - 1;
	assert(mem_pane_end <= mem_end);

	/* In case of window resize */
	if (mem_cursor > mem_pane_end) mem_cursor = mem_pane_end;

	drawUndoList();
	positionCursor(1);
}




int drawBanner(int line_flags)
{
	const char *pane_name[NUM_PANES] =
	{
		"HEX ","TEXT","CMD "
	};
	const char *cursor_name[NUM_TERM_TYPES][NUM_CURSOR_TYPES] =
	{
		/* Xterm and MacOS */
		{ "BLK ","BAR ","UL  " },

		/* Linux console */
		{ "BLK ","HBLK","UL  " },

		/* Can't change here on a hardware terminal */
		{ "----","----","----" }
	};
	char text[100];
	st_undo *ud;
	char stype;
	int i;

	if (term_width < MIN_TERM_WIDTH || term_height < MIN_TERM_HEIGHT)
	{
		clearScreen();
		errprintf("Terminal size %dx%d too small.\n",
			term_width,term_height);
		return 0;
	}

	if (line_flags & BAN_LINE1)
	{
		clearLine(0);
		locate(0,0);
		snprintf(text,sizeof(text),"%lu/%lu",
			(u_long)(mem_cursor - mem_start),(u_long)file_size);
		colprintf("~RS~BM~FWFile position    :~RS %-20s ",text);
		colprintf("~BB~FWFilename     :~RS %s\n",
			filename ? filename : "");
	}

	if (line_flags & BAN_LINE2)
	{
		clearLine(1);
		locate(0,1);
		colprintf("~BR~FWUndo cnt/val (F5):~RS %-4d",user_undo_cnt);
		if (undo_cnt)
		{
			ud = &undo_list[undo_cnt-1];
			switch(ud->type)
			{
			case UNDO_CHAR:
			case UNDO_DELETE:
				if (flags.lowercase_hex)
				{
					colprintf("~FT,~RS0x%02x/'%c'        ",
						ud->prev_char,
						GET_PRINTABLE(ud->prev_char));
				}
				else
				{
					colprintf("~FT,~RS0x%02X/'%c'        ",
						ud->prev_char,
						GET_PRINTABLE(ud->prev_char));
				}
				break;
			case UNDO_INSERT:
				colprintf("~FT,~RS[insert]        ");
				break;
			case UNDO_STR:
				colprintf("~FT,~RS[S&R]           ");
				break;
			default:
				assert(0);
			}
		}
		else printf("                 ");

		if (flags.search_hex) stype = 'X';
		else if (flags.search_ign_case) stype = 'I';
		else stype = 'T';

		colprintf("~BT~FW%c Search (F6):~RS ",stype);

		if (flags.search_hex)
		{
			if (search_text_len)
			{
				printf("0x");
				for(i=0;i < search_text_len;++i)
				{
					if (flags.lowercase_hex)
						printf("%02x",search_text[i]);
					else
						printf("%02X",search_text[i]);
				}
				putchar('\n');
			}
		}
		else puts(search_text_len ? (char *)search_text : "");
	}

	if (line_flags & BAN_LINE3)
	{
		clearLine(2);
		locate(0,2);
		colprintf("~BY~FWScreen pane (Tab):~RS %s",pane_name[term_pane]);
		colprintf("                 ~BG~FWMode     (F3):~RS %s",
			flags.insert_mode ? "INSERT" : "OVERWRITE");
	}
	if (line_flags & BAN_LINE4)
	{
		clearLine(3);
		locate(0,3);
		colprintf("~BB~FWT&P size & cursor:~RS ");
		snprintf(text,sizeof(text),"%dx%d,%dx%d,%s",
			term_width,
			term_height,
			term_pane_cols,
			term_height - BAN_PANE_HEIGHT - CMD_PANE_HEIGHT - 2,
			cursor_name[term_type][cursor_type]);

		colprintf("%-20s ~BM~FWS&R count    :~RS %d",text,sr_cnt);
	}
	return 1;
}




void drawCmdPane()
{
	int set_cursor_pos = 1;
	int i;

	for(i=1;i <= CMD_PANE_HEIGHT;++i) clearLine(term_div_y+i);

	cmd_y = term_div_y + 1;
	locate(0,cmd_y);
	colprintf("~RS%s",PROMPT);

	cmd_x = strlen(PROMPT);

	if (user_cmd)
	{
		++cmd_x;
		putchar(user_cmd);
	}
	else write(STDOUT_FILENO," \b",2); /* Clear any command char */

	if (cmd_state >= STATE_SAVE_OK) locate(0,term_textbox_y);

	switch(cmd_state)
	{
	case STATE_CMD:
		if (user_cmd) locate(0,term_textbox_y);

		switch(user_cmd)
		{
		case 'C':
			colprintf("~FTColour: %s",
				flags.use_colour ? "~FGON~RS" : "OFF");
			break;
		case 'D':
			drawDecodeView();
			break;
		case 'F':
			drawFileInfo();
			break;
		case 'H':
			drawHelp();
			break;	
		case 'I':
		case 'N':
		case 'T':
			if (flags.search_wrapped)
				colprintf("~FGSearch wrapped...");
			break;
		case 'R':
			locate(0,term_textbox_y);
			colprintf("~FTRedraw screen...");
			break;
		case 'V':
			version();
			break;
		default:
			break;
		}
		break;

	case STATE_TEXT:
	case STATE_YN:
		cmd_y = term_textbox_y;
		locate(0,cmd_y);

		/* If user has already entered some text move cursor along */
		cmd_x = cmd_text_len;

		switch(user_cmd)
		{
		case 'A':
			colprintf("~FMSave file as:~RS %s",cmd_text);	
			cmd_x += 14;
			locate(cmd_x,cmd_y);
			break;
		case 'G':
			cmd_x += 21;
			colprintf("~FMGo to file position:~RS %s",cmd_text);
			break;
		case 'I':
			cmd_x += 30;
			colprintf("~FMCase insensitive text search:~RS %s\n",
				cmd_text);
			break;
		case 'Q':
			colprintf("~FMQuit...");
			break;
		case 'S':
			locate(0,term_textbox_y);
			colprintf("~FMSaving file:~RS %s\n",filename);
			break;
		case 'T':
			colprintf("~FMText search:~RS %s\n",cmd_text);
			cmd_x += 13;
			break;
		case 'X':
			colprintf("~FMHex search:~RS %s",cmd_text);
			cmd_x += 12;
			break;
		default:
			/* Use states instead of user_cmd for search and
			   replace as its more logical */
			switch(sr_state)
			{
			case SR_STATE_TEXT1:
				colprintf("~FW~BB*** Text search and replace ***\n");
				colprintf("~FMOld text:~RS %s\n",cmd_text);
				cmd_x += 10;
				locate(cmd_x,++cmd_y);
				break;

			case SR_STATE_TEXT2:
				colprintf("~FW~BB*** Text search and replace ***\n");
				colprintf("~FMOld text:~RS %s\n",search_text);
				colprintf("~FYNew text:~RS %s",cmd_text);
				cmd_y += 2;
				cmd_x = cmd_text_len + 10;
				locate(cmd_x,cmd_y);
				break;

			case SR_STATE_HEX1:
				colprintf("~FW~BM*** Hex search and replace ***\n");
				colprintf("~FMOld hex:~RS %s",cmd_text);
				cmd_x += 9;
				locate(cmd_x,++cmd_y);
				break;

			case SR_STATE_HEX2:
				colprintf("~FW~BM*** Hex search and replace ***\n");
				colprintf("~FMOld hex:~RS %s\n",hex_text);
				colprintf("~FYNew hex:~RS %s",cmd_text);
				cmd_y += 2;
				cmd_x = cmd_text_len + 9;
				locate(cmd_x,cmd_y);
				break;

			default:
				assert(0);
			}
		}
		if (cmd_state == STATE_TEXT) break;

		set_cursor_pos = 0;
		cmd_x = 20;
		cmd_y = term_div_y + 3;
		locate(0,cmd_y);
		colprintf("~FYAre you sure (Y/N)?~RS ");
		break;

	case STATE_SAVE_OK:
		colprintf("~BG~FW*** Saved ***~RS");
		break;

	case STATE_ERR_NOT_FOUND:
		colprintf("~BM~FW*** Not found ***~RS");
		break;

	case STATE_ERR_CMD:
		errprintf("Unknown command '%c'.",user_cmd);
		break;

	case STATE_ERR_SAVE:
		syserrprintf("open");
		break;

	case STATE_ERR_INVALID_PATH:
		errprintf("Invalid path.");
		break;

	case STATE_ERR_INPUT:
		errprintf("Invalid input.");
		break;

	case STATE_ERR_NO_SEARCH_TEXT:
		errprintf("Search text not set.");
		break;

	case STATE_ERR_NO_FILENAME:
		errprintf("Filename not set.");
		break;

	case STATE_ERR_INVALID_HEX_LEN:
		errprintf("Invalid hex length.");
		break;

	case STATE_ERR_MUST_DIFFER:
		errprintf("Search and replace strings must differ.");
		break;

	case STATE_ERR_UNDO:
		colprintf("~BM~FW*** No more undos ***~RS");
		flags.clear_no_undo = 1;
		break;

	case STATE_ERR_DATA_VIEW:
		errprintf("Not enough data following cursor.");
		break;

	default:
		assert(0);
	}
	if (set_cursor_pos) locate(cmd_x,cmd_y);
	fflush(stdout);
}




/*** Can't put inline in drawMain() as its called from file.c and is too
     complicated anyway ***/
void drawUndoList()
{
	/* Oldest to newest change */
	char *undo_col[MAX_UNDO_COL] =
	{
		"FR","FM","FM","FY","FY",
		"FY","FG","FG","FG","FG",
		"FT","FT","FT","FT","FT",
		"FB","FB","FB","FB","FB",
		"FB"
	};
	st_undo *ud;
	u_char *save_cursor;
	u_char *ptr;
	u_char c;
	char *colstr;
	int save_pane;
	int save_right;
	int colpos;
	int cnt;
	int i;
	int j;

	if (!undo_cnt) return;

	/* Draw from oldest first so if we have 2 locations that are the same
	   place we overwrite with newest */
	save_cursor = mem_cursor;
	save_pane = term_pane;
	save_right = flags.cur_hex_right;
	flags.cur_hex_right = 0;
	colpos = undo_cnt - 1;

	for(i=0;i < undo_cnt;++i)
	{
		ud = &undo_list[i];
		ptr = mem_start + ud->pos;
		cnt = (ud->type == UNDO_STR ? ud->str_len : 1);
		
		/* If we're not using colour we don't need to highlight colour
		   in anything, just print the last undo character */
		if (!flags.use_colour && i != undo_cnt - 1) continue;

		/* Can't draw colours for a delete */
		if (ud->type == UNDO_DELETE || 
		    ptr < mem_pane_start || ptr > mem_pane_end)
		{
			--colpos;
			continue;
		}

		for(j=0;j < cnt;++j,++ptr)
		{
			mem_cursor = ptr;
			term_pane = PANE_HEX;
			positionCursor(0);
			colstr = (colpos < MAX_UNDO_COL ? undo_col[colpos] : "FW");
			if (flags.lowercase_hex)
				colprintf("~%s%02x ",colstr,*ptr);
			else
				colprintf("~%s%02X ",colstr,*ptr);

			term_pane = PANE_TEXT;
			positionCursor(0);
			c = *ptr;
			colprintf("~%s%c",colstr,GET_PRINTABLE(c));
		}

		--colpos;
	}
	colprintf("~RS");
	mem_cursor = save_cursor;
	term_pane = save_pane;
	flags.cur_hex_right = save_right;
}




void drawDecodeView()
{
	struct tm *tms;
	uint16_t s1;
	uint16_t s2;
	uint32_t i1;
	uint32_t i2;
	uint64_t l1;
	uint64_t l2;
	u_char *p1;
	u_char *p2;
	char text[50];
	time_t t;
	int len;
	int i;

	if (!mem_decode_view) return;

	locate(0,term_textbox_y);
	colprintf("~BM~FWDecode file position:~RS %lu",
		(u_long)(mem_decode_view - mem_start));
	len = (int)(mem_end - mem_decode_view + 1);
	assert(len > 1);

	p1 = (u_char *)&s1;
	p2 = (u_char *)&s2;

	/* Do 16 bit first. We don't know if the system we're on is big endian
	   or little endian so just call them sys and rev */
	memcpy(p1,mem_cursor,2);
	locate(0,term_textbox_y+1);
	if (decode_page)
		colprintf("~FGU16 system :~RS %u\n",s1);
	else
		colprintf("~FGS16 system :~RS %d\n",(int16_t)s1);

	/* Can't use ntoh*() functions because they don't always
	   do anything depending on the architecture */
	s2 = s1;
	p1[0] = p2[1];
	p1[1] = p2[0];
	if (decode_page)
		colprintf("~FTU16 reverse:~RS %u\n",s1);
	else
		colprintf("~FTS16 reverse:~RS %d\n",(int16_t)s1);

	if (len < 4) return;

	p1 = (u_char *)&i1;
	p2 = (u_char *)&i2;

	/* 32 bit system byte order. */
	memcpy(p1,mem_cursor,4);
	if (decode_page)
		colprintf("~FGU32 system :~RS %u\n",i1);
	else
		colprintf("~FGS32 system :~RS %d\n",(int32_t)i1);

	if (decode_page)
	{
		/* U32 date system order */
		locate(DEC_COL2_X,term_textbox_y+3);
		t = (time_t)i1;
		colprintf("~FGU32 date sys:~RS ",t);
		if ((tms = gmtime(&t)))
		{
			strftime(text,sizeof(text),"%F %T UTC",tms);
			puts(text);
		}
		else puts("<invalid>");
	}

	/* 32 bit reverse order */
	locate(0,term_textbox_y+4);
	i2 = i1;
	for(i=0;i < 4;++i) p1[i] = p2[3-i];
	if (decode_page)
		colprintf("~FTU32 reverse:~RS %u\n",i1);
	else
		colprintf("~FTS32 reverse:~RS %d\n",(int32_t)i1);

	if (decode_page)
	{
		/* U32 date reverse order */
		locate(DEC_COL2_X,term_textbox_y+4);
		colprintf("~FTU32 date rev:~RS ");
		t = (time_t)i1;
		if ((tms = gmtime(&t)))
		{
			strftime(text,sizeof(text),"%F %T UTC",tms);
			puts(text);
		}
		else puts("<invalid>");
	}

	if (len < 8) return;

	/* 64 bit system */
	p1 = (u_char *)&l1;
	p2 = (u_char *)&l2;
	memcpy(p1,mem_cursor,8);
	locate(DEC_COL2_X,term_textbox_y+1);
	if (decode_page)
		colprintf("~FGU64 system  :~RS %llu\n",l1);
	else
		colprintf("~FGS64 system  :~RS %lld\n",(uint64_t)l1);

	/* 64 bit reverse */
	l2 = l1;
	for(i=0;i < 8;++i) p1[i] = p2[7-i];
	locate(DEC_COL2_X,term_textbox_y+2);
	if (decode_page)
		colprintf("~FTU64 reverse :~RS %llu\n",l1);
	else
		colprintf("~FTS64 reverse :~RS %lld\n",(uint64_t)l1);

	locate(0,term_textbox_y+5);
	colprintf("~FGsys~RS = system byte order, ~FTrev~RS = reverse order. ~FGPress 'D' for next...~RS");
}




/*** Draw the lines dividing the data panes from the banner and command 
     panes ***/
void drawHorizontalLines()
{
	int div_y;
	int cols;
	int i;
	int y;
	int w;

	w = term_pane_cols * 4 + 2;
	div_y = term_pane_cols * 3;

	for(y=BAN_PANE_HEIGHT;;y=term_div_y)
	{
		locate(0,y);
		colprintf("~BT~FT");
		for(i=cols=0;i < w;++i)
		{
			if (i == div_y) 
			{
				putchar('-');
				cols = 0;
			}
			/* Hex pane */
			else if (i < div_y)
			{
				if (!(i % 3))
				{
					if (!(++cols % 4))
						colprintf("~BB|~BT");
					else
						putchar('-');
				}
				else putchar('-');
			}
			/* Text pane */
			else if (++cols > 1 && cols % 4 == 1)
				colprintf("~BM|~BT");
			else
				putchar('-');
		}
		colprintf("~RS");
		if (y == term_div_y)  break;
	}
	locate(0,BAN_PANE_HEIGHT+1);
}




void drawFileInfo()
{
	colprintf("~BM~FW*** File information ***\n");
	colprintf("~FYOriginal size:~RS %lu bytes\n",file_stat.st_size);
	colprintf("~FGCurrent size :~RS %lu bytes\n",file_size);
	colprintf("~FMTotals:~RS\n");
	colprintf("   ~FTInserts:~RS %-5d  ~FTDeletes:~RS %d\n",
		total_inserts,total_deletes);
	colprintf("   ~FTUpdates:~RS %-5d  ~FTUndos  :~RS %d",
		total_updates,total_undos);
}




void drawHelp()
{
	switch(help_page)
	{
	case HELP_COM_1:
		colprintf("~BM~FW*** Commands page 1 ***\n\n");
		colprintf("~FTA:~RS Save file as  ~FTC:~RS Toggle colour  ~FTD:~RS Decode            ~FTF:~RS File info\n");
		colprintf("~FTG:~RS Goto pos      ~FTH:~RS Help           ~FTI:~RS Text search (CI)  ~FTL:~RS Toggle hex case\n");
		colprintf("~FTM:~RS Toggle mode   ~FTN:~RS Find next      ~FTQ:~RS Quit              ~FTR:~RS Redraw\n");
		colprintf("~FGPress 'H' again for more commands...~RS");
		break;

	case HELP_COM_2:
		colprintf("~BM~FW*** Commands page 2 ***\n\n");
		colprintf("~FTS:~RS Save file  ~FTT:~RS Text search  ~FTV:~RS Version  ~FTX:~RS Hex search\n");
		colprintf("~FTY:~RS Text search & replace      ~FTZ:~RS Hex search & replace\n\n");
		colprintf("~FGPress 'H' again for function keys...~RS");
		break;

	case HELP_FKEYS:
		colprintf("~BM~FW*** Function keys ***\n\n");
		colprintf("~FYF1:~RS Page up   ~FYF2:~RS Page down   ~FYF3:~RS Toggle mode  ~FYF4:~RS Delete\n");
		colprintf("~FYF5:~RS Undo      ~FYF6:~RS Search      ~FYF7:~RS Decode       ~FYF8:~RS Cycle cursor types\n\n");
		colprintf("~FGPress 'H' again for commands...~RS");
	}
}
