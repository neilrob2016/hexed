#include "globals.h"

void sighandler(int sig)
{
	if (sig == SIGINT || sig == SIGQUIT)
	{
		puts("Quit");
		doExit(sig);
	}
}




void initSignals(void)
{
	signal(SIGINT,sighandler);
	signal(SIGQUIT,sighandler);
	signal(SIGWINCH,sighandler);
}
