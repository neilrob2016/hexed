#include "globals.h"

#define FREE(M) if (M) free(M)

void initUndo(int do_free)
{
	int i;
	if (do_free)
	{
		for(i=0;i < MAX_UNDO;++i) FREE(undo_list[i].prev_str);
	}
	oldest_undo_pos = undo_cnt = 0;
	bzero(undo_list,sizeof(undo_list));
	mem_undo_reset = NULL;
	undo_reset_str_len = 1;
}




/*** If str_len is set then its a search and replace undo */
void addUndo(u_char *mem_pos, int str_len)
{
	struct st_undo *ul;
	u_char *str;

	next_undo_pos = (oldest_undo_pos + undo_cnt) % MAX_UNDO;

	if (undo_cnt < MAX_UNDO) ++undo_cnt;
	else
	{
		/* This entry will be invalidated soon so save what we need */
                mem_undo_reset = undo_list[oldest_undo_pos].mem_pos;
		undo_reset_str_len = undo_list[oldest_undo_pos].str_len;

		oldest_undo_pos = (oldest_undo_pos + 1) % MAX_UNDO;
	}
	ul = &undo_list[next_undo_pos];
	FREE(ul->prev_str);
	bzero(ul,sizeof(struct st_undo));

	ul->mem_pos = mem_pos;
	ul->cur_hex_right = flags.cur_hex_right;
	if (str_len)
	{
		str = (u_char *)malloc(str_len);
		assert(str);
		memcpy(str,mem_pos,str_len);
		ul->prev_str = str;
		ul->str_len = str_len;
	}
	else ul->prev_char = *mem_pos;
}




void undo()
{
	struct st_undo *ul;
	if (undo_cnt)
	{
		ul = &undo_list[next_undo_pos];
		mem_cursor = ul->mem_pos;
		if (ul->str_len)
			memcpy(mem_cursor,ul->prev_str,ul->str_len);	
		else
			*mem_cursor = ul->prev_char;
		flags.cur_hex_right = ul->cur_hex_right;
		--next_undo_pos;
		--undo_cnt;
		++total_undos;

		if (next_undo_pos < 0) next_undo_pos += MAX_UNDO;
		if (mem_cursor < mem_pane_start || mem_cursor > mem_pane_end)
			setPaneStart(mem_cursor);
	}
	else cmd_state = STATE_ERR_UNDO;
}
