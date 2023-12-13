#include "globals.h"

#define UNDO_MALLOC_INC 5

void initUndo()
{
	undo_cnt = 0;
	undo_malloc_cnt = 0;
	undo_list = NULL;
	mem_undo_reset = NULL;
	undo_reset_str_len = 1;
}




/*** If str_len is set then its a search and replace undo */
void addUndo(int type, u_char *mem_pos, int str_len)
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

	ud->mem_pos = mem_pos;
	ud->cur_hex_right = flags.cur_hex_right;
	ud->type = type;

	switch(type)
	{
	case UNDO_INSERT:
		break;
	case UNDO_CHAR:
	case UNDO_DELETE:
		ud->prev_char = *mem_pos;
		break;
	case UNDO_STR:
		str = (u_char *)malloc(str_len);
		assert(str);
		memcpy(str,mem_pos,str_len);
		ud->prev_str = str;
		ud->str_len = str_len;
		break;
	default:
		assert(0);
	}
	++undo_cnt;
}




/*** Called after a realloc insert ***/
void resetUndoPointersAfterRealloc(u_char *new_mem_start)
{
	int i;

	for(i=0;i < undo_cnt;++i)
		undo_list[i].mem_pos = new_mem_start + (undo_list[i].mem_pos - mem_start);
}




void undo()
{
	st_undo *ud;

	if (undo_cnt)
	{
		ud = &undo_list[undo_cnt-1];
		mem_cursor = ud->mem_pos;
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
			deleteAtCursorPos(0);
			break;
		case UNDO_DELETE:
			insertAtCursorPos(ud->prev_char,0);
			break;
		default:
			assert(0);
		}
		flags.cur_hex_right = ud->cur_hex_right;
		--undo_cnt;
		++total_undos;

		if (mem_cursor < mem_pane_start || mem_cursor > mem_pane_end)
			setPaneStart(mem_cursor);
	}
	else
	{
		prev_cmd_state = cmd_state;
		cmd_state = STATE_ERR_UNDO;
	}
}
