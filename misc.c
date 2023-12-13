#include "globals.h"

void clearCommandText()
{
	cmd_text[0] = 0;
	cmd_text_len = 0;
	user_cmd = 0;
}




void resetCommand()
{
	clearCommandText();
	prev_cmd_state = cmd_state = STATE_CMD;
	sr_state = SR_STATE_NONE;

	/* Want to get rid of decode highlighting when we reset the command */
	if (mem_decode_view)
	{
		mem_decode_view = NULL;
		drawScreen();
	}
}




void version()
{
        colprintf("~BM~FW*** HEXED ***\n\n");
        colprintf("~FTCopyright (C) Neil Robertson 2022-2023\n\n");
	colprintf("~FYVersion~RS   : %s\n",VERSION);
	colprintf("~FGBuild date~RS: %s",BUILD_DATE);
}




void doExit(int code)
{
	tcsetattr(STDIN,TCSANOW,&saved_tio);
	colprintf("~RS");
	exit(code);
}
