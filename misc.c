#include "globals.h"

void resetCommand()
{
	cmd_state = STATE_CMD;
	user_cmd = 0;
	cmd_text[0] = 0;
	cmd_text_len = 0;

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
        colprintf("~FTCopyright (C) Neil Robertson 2022\n\n");
	colprintf("~FYVersion~RS   : %s\n",VERSION);
	colprintf("~FGBuild date~RS: %s",BUILD_DATE);
}




void doExit(int code)
{
	tcsetattr(STDIN,TCSANOW,&saved_tio);
	colprintf("~RS");
	exit(code);
}
