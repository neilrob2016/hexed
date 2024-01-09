#include "globals.h"

#define ALLOC_BLOCK_LEN 10
#define FILE_BLOCK_LEN  1000

/*** This would use memory mapping except that is awkward when you want to
     insert data into the file and enlarge it ***/
void mapFile()
{
	u_char *ptr;
	u_long dist;
	ssize_t rlen;
	ssize_t len;
	int fd;

	if (filename)
	{
		printf("Opening file \"%s\"... ",filename);
		fflush(stdout);

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
		file_malloc_size = (u_long)file_stat.st_size;
	}
	else
	{
		printf("Initialising memory... ");
		bzero(&file_stat,sizeof(file_stat));
		file_stat.st_mode = 0644; /* For file save option */
		file_stat.st_size = 1;
		file_malloc_size = ALLOC_BLOCK_LEN;

		/* If mode not set to overwrite in RC file then set to insert
		   mode (not much point in having overwrite mode with 1 byte) */
		if (!flags.rc_overwrite_mode_set) flags.insert_mode = 1;
	}
	printok();

	file_size = (u_long)file_stat.st_size;
	mem_start = (u_char *)malloc(file_malloc_size);
	assert(mem_start);
	mem_end = mem_start + file_stat.st_size - 1;
	mem_cursor = mem_pane_start = mem_start;

	if (!filename)
	{
		/* Init single allocated byte to zero */
		*mem_start = 0;
		return;
	}

	for(ptr=mem_start;ptr <= mem_end;ptr+=len)
	{
		dist = (u_long)(mem_end - ptr + 1);
		len = (size_t)(dist < FILE_BLOCK_LEN ? dist : FILE_BLOCK_LEN);
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




int resolveUserHomeDir(char *name, char *path)
{
	struct passwd *pwd;
	char *ptr;
	char c;

	/* Find end of username */
	for(ptr=name;*ptr && *ptr != '/';++ptr);

	/* Need more than just the username */
	if (!*ptr) return 0;

	/* Get home dir */
	c = *ptr;
	*ptr = 0;
	pwd = getpwnam(name);
	*ptr = c;

	if (pwd)
	{
		snprintf(path,PATH_MAX,"%s/%s",pwd->pw_dir,ptr+1);
		return 1;
	}
	return 0;
}




int saveFile(char *name)
{
	char path[PATH_MAX];
	u_char *ptr;
	u_long dist;
	u_long len;
	int err;
	int fd;

	if (name[0] == '~')
	{
		/* Resolve our or another users home dir path */
		if (name[1] == '/')
			snprintf(path,PATH_MAX,"%s/%s",home_dir,name+2);
		else if (!resolveUserHomeDir(name+1,path))
			return STATE_ERR_INVALID_PATH;
	}
	else snprintf(path,PATH_MAX,"%s",name);

	if ((fd = open(path,O_WRONLY | O_CREAT,file_stat.st_mode)) == -1)
		return STATE_ERR_SAVE;

	setFileName(path);
	drawBanner(BAN_LINE1);

	for(ptr=mem_start;ptr <= mem_end;ptr += len)
	{
		dist = (u_long)(mem_end - ptr + 1);
		len = (dist < FILE_BLOCK_LEN ? dist : FILE_BLOCK_LEN);
		if (write(fd,ptr,len) == -1)
		{
			err = errno;
			close(fd);
			errno = err;
			return STATE_ERR_SAVE;
		}
	}
	close(fd);
	
	return STATE_SAVE_OK;
}


/******************************* Update data *******************************/

void changeFileData(u_char c)
{
	assert(mem_cursor >= mem_start && mem_cursor <= mem_end);

	if (term_pane == PANE_TEXT)
	{
		if (!IS_PRINTABLE(c)) return;
		if (flags.insert_mode) insertAtCursorPos(c,1);
		else
		{
			addUndo(UNDO_CHAR,mem_cursor,0,1);
			*mem_cursor = c;
		}
		++total_updates;

		/* scrollDown() calls drawMain() which calls drawUndoList()
		   so don't need to call it manually here */
		if (mem_cursor < mem_end && ++mem_cursor > mem_pane_end)
			scrollDown();
		else
		{
			/* Update Undo info */
			drawUndoList();
			drawBanner(BAN_LINE2);
			if (user_cmd == 'F') drawCmdPane();
			positionCursor(1);
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
		addUndo(UNDO_CHAR,mem_cursor,0,1);
		/* 2nd/right nibble of hex value */
		*mem_cursor = (c & 0x0F) | (*mem_cursor & 0xF0);

		/* Have we gone off the bottom? */
		if (mem_cursor < mem_end && ++mem_cursor > mem_pane_end)
			scrollDown();
		else
		{
			drawUndoList();
			if (user_cmd == 'F') drawCmdPane();
		}
	}
	else
	{
		c = (c << 4) | (*mem_cursor & 0x0F);
		if (flags.insert_mode) insertAtCursorPos(c,1);
		else
		{
			addUndo(UNDO_CHAR,mem_cursor,0,1);
			*mem_cursor = c;
			drawUndoList();
		}
		if (user_cmd == 'F') drawCmdPane();
	}
	flags.cur_hex_right = !flags.cur_hex_right;
	++total_updates;

	drawBanner(BAN_LINE2);
	positionCursor(1);
}




/*** Insert zero at the current cursor position and shift rest of file up
     1 character ***/
void insertAtCursorPos(u_char c, int add_undo)
{
	u_char *ptr;

	/* See if we need to realloc first */
	if (file_size == file_malloc_size)
	{
		file_malloc_size += ALLOC_BLOCK_LEN;
		ptr = (u_char *)realloc(mem_start,file_malloc_size);
		assert(ptr);

		/* Reset all pointers */
		resetUndoPointersAfterRealloc(ptr);
		mem_cursor = ptr + (mem_cursor - mem_start);
		mem_pane_start = ptr + (mem_pane_start - mem_start);
		mem_start = ptr;
		mem_end = ptr + file_size - 1;
	}

	/* Move everything up 1 char from the cursor position */
	for(ptr=mem_end+1;ptr > mem_cursor;--ptr) *ptr = *(ptr-1);
	*mem_cursor = c;
	++file_size;
	++mem_end;
	++total_inserts;

	if (add_undo) addUndo(UNDO_INSERT,mem_cursor,0,1);
	mem_decode_view = NULL;

	/* Sets mem_pane_end */
	drawMain();
}




/*** Delete the character at the cursor position. We don't change the size of
     the allocated memory in case the user wants to do an insert elsewhere
     hence file_malloc_size unchanged ***/
void deleteAtCursorPos(int add_undo)
{
	u_char *ptr;

	if (mem_start >= mem_end) return;

	/* Leave 1 char left */
	if (add_undo) addUndo(UNDO_DELETE,mem_cursor,0,1);

	for(ptr=mem_cursor;ptr < mem_end;++ptr) *ptr = *(ptr+1);
	--mem_end;
	if (mem_cursor > mem_end) mem_cursor = mem_end;
	if (mem_cursor < mem_pane_start) setPaneStart(mem_cursor);
	--file_size;
	++total_deletes;

	/* Sets mem_pane_end */
	drawMain();
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




u_char *findSearchText(u_char *start)
{
	u_char *ptr;
	u_char *last;
	int (*fptr)(const void *s1, const void *s2, size_t len);

	last = mem_end - search_text_len + 1;
	if (last < mem_start) return NULL;

	flags.search_wrapped = 0;
	mem_search_find_start = NULL;
	mem_search_find_end = NULL;
	fptr = flags.search_ign_case ? memcasecmp : memcmp;

	/* Search from current cursor position and wrap around */
	for(ptr=start;!flags.search_wrapped || ptr < start;++ptr)
	{
		/* If we're at the end go to the start of memory */
		if (ptr > last)
		{
			ptr = mem_start;
			flags.search_wrapped = 1;
		}

		/* Check for match */
		if (!fptr(ptr,search_text,search_text_len)) return ptr;
	}
	return NULL;
}




/*** Convert the hex values in cmd_text and put them in data ***/
void hexToBinary(u_char *data, int *data_len)
{
	u_char *ptr;
	u_char *end;
	u_char value;
	int dlen;
	int i;

	assert(!(cmd_text_len % 2));

	end = (u_char *)cmd_text + cmd_text_len;
	dlen = 0;

	for(ptr=(u_char *)cmd_text;ptr < end;)
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
		data[dlen++] = value;
		data[dlen] = 0;
	}
	*data_len = dlen;
	flags.search_hex = 1;
}




void findText()
{
	u_char *ptr;
	u_char *mem_prev;
	u_char *search_start;

	search_start = mem_cursor;

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
		/* Move it up one so we don't search from where we've already
		   found something */
		if (search_start < mem_end)
			++search_start;
		else
			search_start = mem_start;
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
		hexToBinary(search_text,&search_text_len);
		break;
	default:
		assert(0);
	}
	mem_prev = mem_search_find_start;

	if (!(ptr = findSearchText(search_start)))
	{
		resetCommand();
		if (mem_prev) drawMain();
		cmd_state = STATE_ERR_NOT_FOUND;
		return;
	}

	/* Found something */
	mem_search_find_start = ptr;
	mem_search_find_end = ptr + search_text_len - 1;
	mem_cursor = ptr;
	if (user_cmd == 'T')
		term_pane = PANE_TEXT;
	else if (user_cmd == 'X')
		term_pane = PANE_HEX;

	if (ptr < mem_pane_start || ptr > mem_pane_end)
		setPaneStart(ptr);
	drawMain();
	cmd_state = STATE_CMD;
}




void searchAndReplace()
{
	u_char *ptr;
	u_char *search_start;
	int undo_seq_start;

	sr_cnt = 0;
	flags.search_hex = 0;

	if (!cmd_text_len)
	{
		resetCommand();
		sr_state = SR_STATE_NONE;
		return;
	}

	switch(sr_state)
	{
	case SR_STATE_TEXT1:
		strcpy((char *)search_text,cmd_text);
		search_text_len = cmd_text_len;
		clearCommandText();
		++sr_state;
		return;

	case SR_STATE_TEXT2:
		if (cmd_text_len != search_text_len)
		{
			resetCommand();
			cmd_state = STATE_ERR_MUST_BE_SAME_LEN;
			return;
		}
		strcpy((char *)replace_text,cmd_text);
		replace_text_len = cmd_text_len;
		break;

	case SR_STATE_HEX1:
		if (cmd_text_len % 2)
		{
			resetCommand();
			cmd_state = STATE_ERR_INVALID_HEX_LEN;
			return;
		}
		strcpy((char *)hex_text,cmd_text);
		hex_text_len = cmd_text_len;
		hexToBinary(search_text,&search_text_len);
		clearCommandText();
		++sr_state;
		return;

	case SR_STATE_HEX2:
		if (cmd_text_len != hex_text_len)
		{
			resetCommand();
			cmd_state = STATE_ERR_MUST_BE_SAME_LEN;
			return;
		}
		strcpy((char *)replace_text,cmd_text);
		hexToBinary(replace_text,&replace_text_len);
		break;

	default:
		assert(0);
	}

	/* The search string and replace strings must differ */
	if (!memcmp(search_text,replace_text,replace_text_len))
	{
		resetCommand();
		cmd_state = STATE_ERR_MUST_DIFFER;
		return;
	}

	/* We've got our search and replace text so lets do some replacing */
	undo_seq_start = 1;
	flags.search_ign_case = 0;

	for(search_start=mem_cursor;
	    (ptr = findSearchText(search_start));++sr_cnt)
	{
		addUndo(UNDO_STR,ptr,replace_text_len,undo_seq_start);
		undo_seq_start = 0;
		memcpy(ptr,replace_text,replace_text_len);
		search_start = ptr + 1;
	}
	drawMain();
	resetCommand();
	sr_state = SR_STATE_NONE;
	if (sr_cnt)
	{
		cmd_state = STATE_UPDATE_COUNT;
		change_cnt = sr_cnt;
	}
	else cmd_state = STATE_ERR_NOT_FOUND;
}
