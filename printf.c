#include "globals.h"

#define NUM_CODES      21
#define RESET_CODE    0


void errprintf(const char *fmt, ...)
{
	va_list args;
	colprintf("~BR~FWERROR:~RS ");
	va_start(args,fmt);
	vprintf(fmt,args);
	va_end(args);
	beep();
}




void syserrprintf(char *func)
{
	colprintf("~BR~FWERROR:~RS~FT %s()~RS: %s\n",func,strerror(errno));
	beep();
}




/*** Print a string in colour if it has embedded colour commands. Taken
     from Nine Mens Morris with some updates. ***/
void colprintf(const char *fmt, ...)
{
	/* Static so we don't keep allocing and deleting memory each time the
	   function is called */
	static char *newfmt = NULL;
	static char *output = NULL;
	static int fmt_alloc = 0;
	static int out_alloc = 0;
	const char *ansitag[NUM_CODES] = 
	{
		"RS","OL","UL","LI","RV",

		"FK","FR","FG","FY",
		"FB","FM","FT","FW",

		"BK","BR","BG","BY",
		"BB","BM","BT","BW"
	};
	const char *ansicode[NUM_CODES] =
	{
		/* Non colour actions */
		"\033[0m", "\033[1m", "\033[4m", "\033[5m", "\033[7m",

		/* Foreground colour */
		"\033[30m","\033[31m","\033[32m","\033[33m",
		"\033[34m","\033[35m","\033[36m","\033[37m",

		/* Background colour */
		"\033[40m","\033[41m","\033[42m","\033[43m",
		"\033[44m","\033[45m","\033[46m","\033[47m"
	};
	va_list args;
	char *s1;	
	char *s2;
	int len;
	int print_len;
	int reset_len;
	int out_len;
	int i;

	if (!(print_len = strlen(fmt))) return;

	/* Allocate space for printf formatted string */
	do
	{
		va_start(args,fmt);
		len = print_len * 2;
		if (len > fmt_alloc && !(newfmt = (char *)realloc(newfmt,len)))
		{
			perror("ERROR: colprintf(): realloc() 1");
			doExit(1);
		}
		fmt_alloc = len;

		/* It returns how long a string would have been if enough 
		   space had been available */
		print_len = vsnprintf(newfmt,len,fmt,args);
		va_end(args);
	} while(print_len > len / 2);

	/* Now allocate space for our output string. *5 because we might have
	   to put a reset before a load of newlines. +1 for \0 */
	if (len > (out_alloc - 1) / 5)
	{
		out_alloc = len * 5 + 1;
		if (!(output = (char *)realloc(output,out_alloc)))
		{
			perror("ERROR: colprintf(): realloc() 2");
			doExit(1);
		}
	}
	reset_len = strlen(ansicode[RESET_CODE]);
	out_len = 0;

	/* Parse ~ tags copying the formatted string into output as we do so */
	for(s1=newfmt,s2=output;*s1;++s1,++s2)
	{
		/* Put a reset before every newline since the terminal will
		   make a mess otherwise */
		if (*s1 == '\n' && flags.use_colour)
		{
			memcpy(s2,ansicode[RESET_CODE],reset_len);
			s2 += reset_len;
			out_len += reset_len;
		}

		*s2 = *s1;
		if (*s1 != '~')
		{
			++out_len;
			continue;
		}

		/* Find colour tag and replace it with terminal code */
		for(i=0;i < NUM_CODES;++i)
		{
			if (!strncmp(s1+1,ansitag[i],2))
			{
				s1 += 2;
				if (flags.use_colour)
				{
					len = strlen(ansicode[i]);
					memcpy(s2,ansicode[i],len);
					s2 += (len - 1);
					out_len += len;
				}
				else --s2;
				break;
			}
		}
		if (i == NUM_CODES) ++out_len;
	}

	/* Just in case there's something there */
	fflush(stdout);

	/* Print it */
	write(STDOUT,output,out_len);
}
