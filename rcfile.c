#include "globals.h"

void parseLine(char *line, int linenum);

void parseRCFile()
{
	FILE *fp;
	char text[PATH_MAX];
	char *home;
	int user_file = 0;
	int linenum;

	/* We'll only see the bootup messages if there's an error */
	colprintf("~BM*** HEXED starting ***\n");

	if (!rc_filename)
	{
		/* Should always be set but just in case... */
		if ((home = getenv("HOME")))
		{
			snprintf(text,sizeof(text),"%s/%s",getenv("HOME"),RC_FILENAME);
			rc_filename = text;
		}
		else rc_filename = RC_FILENAME;
	}
	else user_file = 1;

	printf("Opening RC file \"%s\"... ",rc_filename);
	fflush(stdout);

	if (!(fp = fopen(rc_filename,"r")))
	{
		/* Only error if the user gave us the filename */
		if (user_file)
		{
			errprintf("%s\n",strerror(errno));
			exit(1);
		}
		colprintf("~FYWARNING:~RS %s\n",strerror(errno));
		return;
	}
	printok();

	printf("Reading RC file... ");
	fflush(stdout);

	fgets(text,sizeof(text),fp);
	for(linenum=1;!feof(fp);++linenum)
	{
		parseLine(text,linenum);
		fgets(text,sizeof(text),fp);
	}
	fclose(fp);
	printok();
}




void parseLine(char *line, int linenum)
{
	char opt[PATH_MAX];
	char val[PATH_MAX];

	sscanf(line,"%s %s",opt,val);
	if (opt[0] == '#') return;

	if (!strcmp(opt,"mode"))
	{
		if (!strcmp(val,"insert"))
		{
			if (!flags.insert_mode_set) flags.insert_mode = 1;
		}
		else if (!strcmp(val,"overwrite"))
		{
			if (!flags.insert_mode_set)
			{
				flags.insert_mode = 0;
				/* Set to override auto insert mode if no
				   filename given */
				flags.rc_overwrite_mode_set = 1;
			}
		}
		else goto VAL_ERROR;
	}
	else if (!strcmp(opt,"colour"))
	{
		if (!strcmp(val,"on") || !strcmp(val,"yes"))
		{
			if (!flags.use_colour_set) flags.use_colour = 1;
		}
		else if (!strcmp(val,"off") || !strcmp(val,"no"))
		{
			if (!flags.use_colour_set) flags.use_colour = 0;
		}
		else goto VAL_ERROR;
	}
	else if (!strcmp(opt,"termsize"))
	{
		if (!flags.termsize_set) parseTerminalSize(val,linenum);
	}
	else if (!strcmp(opt,"cursor"))
	{
		if (!flags.cursor_set) parseCursorType(val,linenum);
	}
	else if (!strcmp(opt,"pane"))
	{
		if (!flags.pane_set) parseTerminalPane(val,linenum);
	}
	else if (!strcmp(opt,"subchar"))
	{
		if (!flags.subchar_set) parseSubChar(val,linenum);
	}
	else
	{
		errprintf("Unknown option \"%s\" on line %d.\n",opt,linenum);
		exit(1);
	}
	return;

	VAL_ERROR:
	errprintf("Invalid value \"%s\" for option \"%s\" on line %d.\n",
		val,opt,linenum);
	exit(1);
}
