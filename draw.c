#include "globals.h"

#define GET_PRINTABLE(C) (IS_PRINTABLE(C) ? C : substitute_char)

#define PROMPT "Command or TAB ('H' for help): "

void drawDecodeView();
void drawFileInfo();
void drawHelp();
void drawHorizontalLines();


void drawScreen()
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
					colprintf("~BM~FW%02X~RS ",*ptr);
				}
				else if (ptr >= mem_search_find_start &&
				         ptr <= mem_search_find_end)
				{
					colprintf("~RV%02X~RS ",*ptr);
				}
				else printf("%02X ",*ptr);
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
	drawCmdPane();
	positionCursor();
}




int drawBanner(int line_flags)
{
	const char *pane_name[NUM_PANES] =
	{
		"HEX ","TEXT","CMD "
	};
	const char *cursor_name[NUM_TERM_TYPES-1][NUM_CURSOR_TYPES] =
	{
		/* Xterm and MacOS */
		{ "BLOCK","BAR",       "UNDERLINE" },

		/* Linux console */
		{ "BLOCK","HALF_BLOCK","UNDERLINE" }
	};
	u_char undo_value;
	char stype;
	int i;

	if (term_width < MIN_TERM_WIDTH || term_height < MIN_TERM_HEIGHT)
	{
		clearScreen();
		errprintf("Terminal size %dx%d too small.\n",
			term_width,term_height);
		return 0;
	}

	if (line_flags & BAN_LINE_F_F)
	{
		clearLine(0);
		locate(0,0);
		colprintf("~RS~BM~FWFile position    :~RS %-9lu    ~BB~FWFilename     :~RS %s\n",
			(u_long)(mem_cursor - mem_start),filename);
	}

	if (line_flags & BAN_LINE_U_S)
	{
		clearLine(1);
		locate(0,1);
		colprintf("~BR~FWUndo cnt/val (F5):~RS %-2d",undo_cnt);
		if (undo_cnt)
		{
			if (undo_list[next_undo_pos].str_len)
				colprintf("~FT,~RS[S&R]     ");
			else
			{
				undo_value = undo_list[next_undo_pos].prev_char;
				colprintf("~FT,~RS0x%02X/'%c'  ",
					undo_value,GET_PRINTABLE(undo_value));
			}
		}
		else printf("           ");

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
					printf("%02X",search_text[i]);
				putchar('\n');
			}
		}
		else puts(search_text_len ? (char *)search_text : "");
	}

	if (line_flags & BAN_LINE_S_C)
	{
		clearLine(2);
		locate(0,2);
		colprintf("~BY~FWScreen pane (Tab):~RS %s",pane_name[term_pane]);

		/* Not supported by VT types */
		if (term_type != TERM_VT)
		{
			colprintf("         ~BG~FWCursor   (F8):~RS %s",
				cursor_name[term_type][cursor_type]);
		}
	}
	if (line_flags & BAN_LINE_T_SR)
	{
		clearLine(3);
		locate(0,3);
		colprintf("~BB~FWTerm & pane size :~RS %dx%d~FT,~RS%dx%d",
			term_width,
			term_height,
			term_pane_cols,
			term_height - BAN_PANE_HEIGHT - CMD_PANE_HEIGHT - 2);

		locate(32,3);
		colprintf("~BM~FWS&R count    :~RS %d",sr_count);
	}
	return 1;
}




void drawCmdPane()
{
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
	else putchar(' ');

	if (cmd_state >= STATE_SAVE_OK) locate(0,term_textbox_y);

	switch(cmd_state)
	{
	case STATE_CMD:
		locate(0,term_textbox_y);
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
			locate(0,term_textbox_y);
			colprintf("~FMSaving as:~RS %s\n",filename);
			break;
		case 'G':
			cmd_x += 15;
			colprintf("~FMFile position:~RS %s",cmd_text);
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
			cmd_x += 10;
			colprintf("~FMFilename:~RS %s",cmd_text);	
			break;
		case 'T':
			cmd_x += 13;
			colprintf("~FMText search:~RS %s\n",cmd_text);
			break;
		case 'X':
			cmd_x += 12;
			colprintf("~FMHex search:~RS %s",cmd_text);
			break;
		default:
			/* Use states instead of user_cmd for search and
			   replace as its more logical */
			switch(sr_state)
			{
			case SR_STATE_TEXT1:
				cmd_x += 14;
				colprintf("~FMS&R old text:~RS %s\n",cmd_text);
				break;

			case SR_STATE_TEXT2:
				cmd_x += 14;
				colprintf("~FMS&R old text:~RS %s\n",search_text);
				++cmd_y;	
				locate(0,cmd_y);
				cmd_x = cmd_text_len + 14;
				colprintf("~FYS&R new text:~RS %s",cmd_text);
				break;

			case SR_STATE_HEX1:
				cmd_x += 13;
				colprintf("~FMS&R old hex:~RS %s",cmd_text);
				break;

			case SR_STATE_HEX2:
				cmd_x += 13;
				colprintf("~FMS&R old hex:~RS %s",sr_text);
				++cmd_y;	

				locate(0,cmd_y);
				cmd_x = cmd_text_len + 13;
				colprintf("~FYS&R new hex:~RS %s",cmd_text);
				break;

			default:
				assert(0);
			}
		}
		if (cmd_state == STATE_TEXT) break;

		cmd_x = 20;
		cmd_y = term_div_y+3;
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

	case STATE_ERR_INPUT:
		errprintf("Invalid input.");
		break;

	case STATE_ERR_NO_SEARCH_TEXT:
		errprintf("No search text set.");
		break;

	case STATE_ERR_INVALID_HEX_LEN:
		errprintf("Invalid hex length.");
		break;

	case STATE_ERR_MUST_BE_SAME_LEN:
		errprintf("Search and replace strings must be the same length.");
		break;

	case STATE_ERR_MUST_DIFFER:
		errprintf("Search and replace strings must differ.");
		break;

	case STATE_ERR_UNDO:
		errprintf("No more undos.");
		break;

	case STATE_ERR_DATA_VIEW:
		errprintf("Not enough data following cursor.");
		break;

	default:
		assert(0);
	}
	fflush(stdout);
}




/*** Can't put inline in drawScreen() as its called from file.c and is too
     complicated anyway ***/
void drawUndoList()
{
	/* Oldest to newest change */
	char *undo_col[MAX_UNDO] =
	{
		"FB","FB","FB","FB","FB",
		"FB","FB","FB","FB","FB",
		"FB","FB","FB","FB","FB",
		"FT","FT","FT","FT","FT",
		"FG","FG","FG","FG","FY",
		"FY","FY","FR","FR","FM"
	};
	struct st_undo *ul;
	u_char *save_cursor;
	u_char *ptr;
	u_char c;
	char *colstr;
	int save_pane;
	int save_right;
	int pos;
	int colpos;
	int cnt;
	int i;
	int j;

	if (!undo_cnt) return;

	/* Draw from oldest first so if we have 2 values that are the
	   same we overwrite with newest */
	save_cursor = mem_cursor;
	save_pane = term_pane;
	save_right = flags.cur_hex_right;
	flags.cur_hex_right = 0;

	/* Always start at magenta even if we only have 1 entry so far */
	colpos = MAX_UNDO - undo_cnt;

	for(i=0,pos=oldest_undo_pos;i <= undo_cnt;++i)
	{
		ul = &undo_list[pos];
		if (i)
		{
			ptr = ul->mem_pos;
			pos=(pos+1) % MAX_UNDO;
			cnt = (ul->str_len ? ul->str_len : 1);
		}
		else
		{
			/* Have to specifically redraw in standard colour the
			   value thats dropped off the end of the undo list */
			if (!mem_undo_reset) continue;
			ptr = mem_undo_reset;
			cnt = (undo_reset_str_len ? undo_reset_str_len : 1);
		}

		/* If we're not using colour we don't need to highlight colour
		   in anything, just print latest update */
		if (!flags.use_colour && i < undo_cnt) continue;
		if (ptr < mem_pane_start || ptr > mem_pane_end)
		{
			if (i) ++colpos;
			continue;
		}

		for(j=0;j < cnt;++j,++ptr)
		{
			mem_cursor = ptr;
			term_pane = PANE_HEX;
			positionCursor();
			if (i)
			{
				colstr = undo_col[colpos];
				colprintf("~%s%02X ",colstr,*ptr);
			}
			else colprintf("~RS%02X ",*ptr);

			term_pane = PANE_TEXT;
			positionCursor();
			c = *ptr;
			if (i)
			{
				colstr = undo_col[colpos];
				colprintf("~%s%c",colstr,GET_PRINTABLE(c));
			}
			else colprintf("~RS%c",GET_PRINTABLE(c));
		}

		if (i) ++colpos;
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
		colprintf("~FGU16 sys:~RS %u\n",s1);
	else
		colprintf("~FGS16 sys:~RS %d\n",(int16_t)s1);

	/* Can't use ntoh*() functions because they don't always
	   do anything depending on the architecture */
	s2 = s1;
	p1[0] = p2[1];
	p1[1] = p2[0];
	if (decode_page)
		colprintf("~FTU16 rev:~RS %u\n",s1);
	else
		colprintf("~FTS16 rev:~RS %d\n",(int16_t)s1);

	if (len < 4) return;

	p1 = (u_char *)&i1;
	p2 = (u_char *)&i2;

	/* 32 bit system byte order. */
	memcpy(p1,mem_cursor,4);
	if (decode_page)
		colprintf("~FGU32 sys:~RS %u\n",i1);
	else
		colprintf("~FGS32 sys:~RS %d\n",(int32_t)i1);

	if (decode_page)
	{
		/* U32 date system order */
		locate(22,term_textbox_y+3);
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
		colprintf("~FTU32 rev:~RS %u\n",i1);
	else
		colprintf("~FTS32 rev:~RS %d\n",(int32_t)i1);

	if (decode_page)
	{
		/* U32 date reverse order */
		locate(22,term_textbox_y+4);
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
	locate(22,term_textbox_y+1);
	if (decode_page)
		colprintf("~FGU64 system  :~RS %llu\n",l1);
	else
		colprintf("~FGS64 system  :~RS %lld\n",(uint64_t)l1);

	/* 64 bit reverse */
	l2 = l1;
	for(i=0;i < 8;++i) p1[i] = p2[7-i];
	locate(22,term_textbox_y+2);
	if (decode_page)
		colprintf("~FTU64 reverse :~RS %llu\n",l1);
	else
		colprintf("~FTS64 reverse :~RS %lld\n",(uint64_t)l1);

	locate(0,term_textbox_y+5);
	colprintf("~FGsys~RS = system byte order, ~FTrev~RS = reverse order. ~FGPress 'D' again for next...");
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
						colprintf("~BB+~BT");
					else
						putchar('-');
				}
				else putchar('-');
			}
			/* Text pane */
			else if (++cols > 1 && cols % 4 == 1)
				colprintf("~BM+~BT");
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
	if (help_page)
	{
		colprintf("~BM~FW*** Commands ***\n");
		colprintf("~FTA:~RS Save file  ~FTC:~RS Toggle colour  ~FTD:~RS Decode             ~FTF:~RS File info\n");
		colprintf("~FTG:~RS Goto pos   ~FTH:~RS Help           ~FTI:~RS Text search (CI)   ~FTN:~RS Find next\n");
		colprintf("~FTQ:~RS Quit       ~FTR:~RS Redraw         ~FTS:~RS Save as            ~FTT:~RS Text search\n");
		colprintf("~FTV:~RS Version    ~FTX:~RS Hex search     ~FTY:~RS Text search & rep  ~FTZ:~RS Hex search & replace\n");
		colprintf("~FGPress 'H' again for function keys...");
	}
	else
	{
		colprintf("~BM~FW*** Function keys ***\n");
		colprintf("~FYF1:~RS Page up   ~FYF2:~RS Page down   ~FYF3:~RS Insert   ~FYF4:~RS Delete\n");
		colprintf("~FYF5:~RS Undo      ~FYF6:~RS Search      ~FYF7:~RS Decode   ~FYF8:~RS Cursor type\n");
		colprintf("~FGPress 'H' again for commands...");
	}
}
