#include "globals.h"

void sighandler(int sig)
{
	if (sig == SIGINT || sig == SIGQUIT)
	{
		puts("Quit");
		doExit(sig);
	}
}




void initSignals()
{
	signal(SIGINT,sighandler);
	signal(SIGQUIT,sighandler);
	signal(SIGWINCH,sighandler);
}
