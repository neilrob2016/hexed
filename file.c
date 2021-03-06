#include "globals.h"

#define BLOCK_SIZE  1000

/*** This would use memory mapping except that is awkward when you want to
     insert data into the file and enlarge it ***/
void mapFile()
{
	u_char *ptr;
	u_long dist;
	ssize_t rlen;
	ssize_t len;
	int fd;

	if ((fd = open(filename,O_RDONLY)) == -1)
	{
		syserrprintf("open");
		doExit(1);
	}
	if (fstat(fd,&file_stat) == -1)
	{
		syserrprintf("fstat");
		doExit(1);
	}
	file_size = file_malloc_size = (u_long)file_stat.st_size;
	mem_start = (u_char *)malloc(file_malloc_size);
	assert(mem_start);
	mem_end = mem_start + file_stat.st_size - 1;
	mem_cursor = mem_pane_start = mem_start;

	for(ptr=mem_start;ptr <= mem_end;ptr+=len)
	{
		dist = (u_long)(mem_end - ptr + 1);
		len = (size_t)(dist < BLOCK_SIZE ? dist : BLOCK_SIZE);
		rlen = read(fd,ptr,len);
		if (rlen == -1)
		{
			syserrprintf("read");
			doExit(1);
		}
		if (rlen < len)
		{
			errprintf("Read %d bytes, expected %d.\n",rlen,len);
			doExit(1);
		}
	}
	close(fd);
}




void setFileName(char *name)
{
	if (filename) free(filename);
	filename = strdup(name);
	assert(filename);
}




int saveFile(char *name)
{
	u_char *ptr;
	u_long dist;
	u_long len;
	int err;
	int fd;

	if ((fd = open(name,O_WRONLY | O_CREAT,file_stat.st_mode)) == -1)
		return 0;
	setFileName(name);

	for(ptr=mem_start;ptr <= mem_end;ptr += len)
	{
		dist = (u_long)(mem_end - ptr + 1);
		len = (dist < BLOCK_SIZE ? dist : BLOCK_SIZE);
		if (write(fd,ptr,len) == -1)
		{
			err = errno;
			close(fd);
			errno = err;
			return 0;
		}
	}
	close(fd);
	
	return 1;
}


/******************************* Update data *******************************/

void changeFileData(u_char c)
{
	assert(mem_cursor >= mem_start && mem_cursor <= mem_end);
	if (term_pane == PANE_TEXT)
	{
		if (!IS_PRINTABLE(c)) return;

		addUndo(mem_cursor);
		*mem_cursor = c;
		++total_updates;

		drawUndoList();
		if (mem_cursor < mem_end && ++mem_cursor > mem_pane_end)
			scrollDown();
		else
		{
			/* Update Undo banner line */
			drawBanner(BAN_LINE_U_S);
			if (user_cmd == 'F') drawCmdPane();
			positionCursor();
		}
		return;
	}

	assert(term_pane == PANE_HEX);
	c = toupper(c);
	if (c >= 'A' && c <= 'F') c = c - 'A' + 10;
	else if (c >= '0' && c <= '9') c -= '0';
	else return;

	if (flags.cur_hex_right)
	{
		addUndo(mem_cursor);
		/* 2nd/right nibble of hex value */
		*mem_cursor = (c & 0x0F) | (*mem_cursor & 0xF0);
		drawUndoList();

		/* Have we gone off the bottom? */
		if (mem_cursor < mem_end && ++mem_cursor > mem_pane_end)
			scrollDown();
		else if (user_cmd == 'F')
			drawCmdPane();
	}
	else
	{
		addUndo(mem_cursor);
		*mem_cursor = (c << 4) | (*mem_cursor & 0x0F);
		drawUndoList();
		if (user_cmd == 'F') drawCmdPane();
	}
	flags.cur_hex_right = !flags.cur_hex_right;
	++total_updates;

	drawBanner(BAN_LINE_U_S);
	positionCursor();
}




/*** Insert zero at the current cursor position and shift rest of file up
     1 character ***/
void insertAtCursorPos()
{
	u_char *ptr;

	/* See if we need to realloc first */
	if (file_size == file_malloc_size)
	{
		++file_malloc_size;
		ptr = (u_char *)realloc(mem_start,file_malloc_size);
		assert(ptr);

		/* Reset all pointers */
		mem_cursor = ptr + (mem_cursor - mem_start);
		mem_pane_start = ptr + (mem_pane_start - mem_start);
		mem_start = ptr;
		mem_end = ptr + file_size - 1;
	}

	/* Move everything up 1 char from the cursor position */
	for(ptr=mem_end+1;ptr > mem_cursor;--ptr) *ptr = *(ptr-1);
	*mem_cursor = insert_char;
	++file_size;
	++mem_end;
	++total_inserts;

	/* Stored pointers will be invalid now */
	initUndo();
	mem_decode_view = NULL;

	/* Sets mem_pane_end */
	drawScreen();
}




/*** Delete the character at the cursor position. No realloc and 
     file_malloc_size unchanged since in case of insert afterwards we can use
     the already reserved memory ***/
void deleteAtCursorPos()
{
	u_char *ptr;

	/* Leave 1 char left */
	if (mem_start < mem_end) 
	{
		for(ptr=mem_cursor;ptr < mem_end;++ptr) *ptr = *(ptr+1);
		--mem_end;
		if (mem_cursor > mem_end) mem_cursor = mem_end;
		--file_size;
		++total_deletes;

		initUndo();

		/* Sets mem_pane_end */
		drawScreen();
	}
}


/********************************* Searching ********************************/

/*** Returns 0 if it succeeds to be consistent with memcmp() but we don't
     need diff info so just return 1 on failure ***/
int memcasecmp(const void *s1, const void *s2, size_t len)
{
	u_char *c1;
	u_char *c2;
	size_t i;

	if (!s1 || !s2) return 1;

	c1 = (u_char *)s1;
	c2 = (u_char *)s2;
	for(i=0;i < len;++i,++c1,++c2)
	{
		if (toupper(*c1) != toupper(*c2)) break;
	}
	return (i < len);
}




void findText()
{
	u_char *ptr;
	u_char *mem_prev;
	int (*fptr)(const void *s1, const void *s2, size_t len);

	switch(user_cmd)
	{
	case 'I':
	case 'T':
		if (!cmd_text_len) 
		{
			resetCommand();
			return;
		}
		strcpy((char *)search_text,cmd_text);
		search_text_len = cmd_text_len;
		flags.search_hex = 0;
		break;
	case 'N':
		if (!search_text_len) 
		{
			cmd_state = STATE_ERR_NO_SEARCH_TEXT;
			return;
		}
		break;
	case 'X':
		if (!cmd_text_len) 
		{
			resetCommand();
			return;
		}
		if (cmd_text_len % 2)
		{
			resetCommand();
			cmd_state = STATE_ERR_INVALID_HEX_LEN;
			return;
		}
		hexToSearchText();
		break;
	default:
		assert(0);
	}
	flags.search_wrapped = 0;
	mem_prev = mem_search_find_start;
	mem_search_find_start = NULL;
	mem_search_find_end = NULL;
	fptr = flags.search_ign_case ? memcasecmp : memcmp;

	/* Search from current cursor position and wrap around */
	for(ptr=mem_cursor;!flags.search_wrapped || ptr <= mem_cursor;)
	{
		/* If we're at the end go back to the start */
		if (ptr > mem_end - search_text_len)
		{
			ptr = mem_start;
			flags.search_wrapped = 1;
		}
		else ++ptr;

		/* Check for match */
		if (!fptr(ptr,search_text,search_text_len))
		{
			mem_search_find_start = mem_cursor = ptr;
			mem_search_find_end = ptr + search_text_len - 1;
			if (user_cmd == 'T')
				term_pane = PANE_TEXT;
			else if (user_cmd == 'X')
				term_pane = PANE_HEX;

			if (ptr < mem_pane_start || ptr > mem_pane_end)
				setPaneStart(ptr);
			drawScreen();
			cmd_state = STATE_CMD;
			return;
		}
	}

	resetCommand();
	if (mem_prev) drawScreen();
	cmd_state = STATE_NOT_FOUND;
}




/*** Convert the hex values in cmd_text and put them in search_text ***/
void hexToSearchText()
{
	char *ptr;
	char *end;
	u_char value;
	int i;

	assert(!(cmd_text_len % 2));

	end = cmd_text + cmd_text_len;
	search_text_len = 0;

	for(ptr=cmd_text;ptr < end;)
	{
		value = 0;
		for(i=0;i < 2;++i,++ptr)
		{
			if (isdigit(*ptr))
				value |= *ptr - '0';
			else 
				value |= *ptr - 'A' + 10;
			if (!i) value <<= 4;
		}
		search_text[search_text_len++] = value;
		search_text[search_text_len] = 0;
	}
	flags.search_hex = 1;
}
