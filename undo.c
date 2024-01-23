#include "globals.h"

#define UNDO_MALLOC_INC 5

void initUndo()
{
	undo_cnt = 0;
	undo_malloc_cnt = 0;
	user_undo_cnt = 0;
	undo_list = NULL;
	mem_undo_reset = NULL;
	undo_reset_str_len = 1;
}




/*** If str_len is set then its a search and replace undo */
void addUndo(int type, u_char *mem_pos, int str_len, int seq_start)
{
	st_undo *ud;
	u_char *str;

	if (flags.clear_no_undo)
	{
		clearLine(cmd_y+1);
		flags.clear_no_undo = 0;
		if (cmd_state == STATE_ERR_UNDO) cmd_state = prev_cmd_state;
	}

	/* This will grow forever but the chances of memory running out are
	   pretty slim ***/
	if (undo_malloc_cnt == undo_cnt)
	{
		undo_malloc_cnt += UNDO_MALLOC_INC;
		undo_list = (st_undo *)realloc(undo_list,sizeof(st_undo) * undo_malloc_cnt);
		assert(undo_list);
	}
	ud = &undo_list[undo_cnt];
	bzero(ud,sizeof(st_undo));

	ud->seq_start = seq_start;
	ud->pos = (size_t)(mem_pos - mem_start);
	ud->cur_hex_right = flags.cur_hex_right;
	ud->type = type;

	switch(type)
	{
	case UNDO_INSERT:
		break;
	case UNDO_CHAR:
	case UNDO_DELETE:
		ud->prev_char = *(mem_start + ud->pos);
		break;
	case UNDO_STR:
		str = (u_char *)malloc(str_len);
		assert(str);
		memcpy(str,mem_start + ud->pos,str_len);
		ud->prev_str = str;
		ud->str_len = str_len;
		break;
	default:
		assert(0);
	}
	++undo_cnt;
	user_undo_cnt += seq_start;
}




/*** Because the "pos" stored in the undo is an absolute file position, when
     we insert or update into the file the coloured undo list will be in the
     wrong position after the updated position unless we modify it ***/
void updateUndoPositions(int add)
{
	/* Get position to add from */
	size_t pos = (size_t)(mem_cursor - mem_start);
	int i;

	for(i=0;i < undo_cnt;++i)
		if (undo_list[i].pos > pos) undo_list[i].pos += add;
}




void undo()
{
	st_undo *ud;
	size_t diff;
	int cnt = 0;

	if (!undo_cnt)
	{
		prev_cmd_state = cmd_state;
		cmd_state = STATE_ERR_UNDO;
		return;
	}

	/* Retain the same relative position in the file after an undo */
	if (flags.retain_preundo_pos)
		diff = (size_t)(mem_cursor - mem_start);

	/* Loop until we get back to the start of the sequence. Sequences exist
	   because multiple undos can be created for a single search and 
	   replace */
	do
	{
		assert(undo_cnt);
		ud = &undo_list[undo_cnt-1];
		mem_cursor = mem_start + ud->pos;
		switch(ud->type)
		{
		case UNDO_CHAR:
			*mem_cursor = ud->prev_char;
			break;
		case UNDO_STR:
			memcpy(mem_cursor,ud->prev_str,ud->str_len);	
			free(ud->prev_str);
			ud->prev_str = NULL;
			break;
		case UNDO_INSERT:
			deleteAtCursorPos(0,1);
			break;
		case UNDO_DELETE:
			insertAtCursorPos(ud->prev_char,0,0);
			break;
		default:
			assert(0);
		}
		flags.cur_hex_right = ud->cur_hex_right;
		--undo_cnt;
	} while(!ud->seq_start);
	++cnt;

	if (flags.retain_preundo_pos)
	{
		mem_cursor = mem_start + diff;
		if (mem_cursor > mem_end) mem_cursor = mem_end;
	}

	/* Sets mem_pane_end */
	drawMain();
	if (mem_cursor < mem_pane_start || mem_cursor > mem_pane_end)
		setPaneStart(mem_cursor);

	total_undos += cnt;
	--user_undo_cnt;
}
