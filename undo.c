#include "globals.h"


void initUndo()
{
	oldest_undo_pos = undo_cnt = 0;
	bzero(undo_list,sizeof(undo_list));
	mem_undo_reset = NULL;
}




void addUndo(u_char *mem_pos)
{
	next_undo_pos = (oldest_undo_pos + undo_cnt) % MAX_UNDO;

	if (undo_cnt < MAX_UNDO) ++undo_cnt;
	else
	{
		/* Save pointer to change it back to default colour */
                mem_undo_reset = undo_list[oldest_undo_pos].mem_pos;
		oldest_undo_pos = (oldest_undo_pos + 1) % MAX_UNDO;
	}
	undo_list[next_undo_pos].mem_pos = mem_pos;
	undo_list[next_undo_pos].prev_val = *mem_pos;
	undo_list[next_undo_pos].cur_hex_right = flags.cur_hex_right;
}




void undo()
{
	if (undo_cnt)
	{
		mem_cursor = undo_list[next_undo_pos].mem_pos;
		*mem_cursor = undo_list[next_undo_pos].prev_val;
		flags.cur_hex_right = undo_list[next_undo_pos].cur_hex_right;
		--next_undo_pos;
		--undo_cnt;
		++total_undos;

		if (next_undo_pos < 0) next_undo_pos += MAX_UNDO;
		if (mem_cursor < mem_pane_start || mem_cursor > mem_pane_end)
			setPaneStart(mem_cursor);
	}
	else cmd_state = STATE_ERR_UNDO;
}
