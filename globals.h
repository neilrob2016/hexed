#include <stdio.h>
#include <stdlib.h>
#include <stdarg.h>
#include <stdint.h>
#include <string.h>
#include <signal.h>
#include <unistd.h>
#include <termios.h>
#include <errno.h>
#include <assert.h>
#include <fcntl.h>
#include <ctype.h>
#include <time.h>
#include <sys/types.h>
#include <sys/ioctl.h>
#include <sys/select.h>
#include <sys/stat.h>

#include "build_date.h"

#define VERSION "20220921"

#define STDIN           0
#define STDOUT          1
#define BAN_PANE_HEIGHT 4
#define CMD_PANE_HEIGHT 7
#define CMD_X_POS       9
#define CMD_TEXT_SIZE   100
#define MAX_UNDO        25
#define INSERT_CHAR     ' '
#define SUBSTITUTE_CHAR '.'
#define ASCII_DEL       127
#define MIN_TERM_WIDTH  55
#define MIN_TERM_HEIGHT (BAN_PANE_HEIGHT + CMD_PANE_HEIGHT + 3)

#define IS_PRINTABLE(C) (C > 31 && C < ASCII_DEL)

#ifdef MAINFILE
#define EXTERN 
#else
#define EXTERN extern
#endif

enum
{
	PANE_HEX,
	PANE_TEXT,
	PANE_CMD,

	NUM_PANES
};

enum
{
	CUR_BLOCK,
	CUR_HALF_BLOCK_BAR, /* Linux console doesn't support a bar */
	CUR_UNDERLINE,

	NUM_CURSOR_TYPES
};

enum
{
	TERM_XTERM,
	TERM_LINUX_CONSOLE,
	TERM_VT,

	NUM_TERM_TYPES
};

/* Banner lines */
enum
{
	BAN_LINE_F_F  = 1,
	BAN_LINE_U_S  = 2,
	BAN_LINE_S_C  = 4,
	BAN_LINE_T_SR = 8,
	ALL_BAN_LINES = 0xF
};

enum
{
	STATE_CMD,
	STATE_TEXT,
	STATE_YN,
	STATE_SAVE_OK,

	STATE_ERR_NOT_FOUND,
	STATE_ERR_CMD,
	STATE_ERR_SAVE,
	STATE_ERR_INPUT,
	STATE_ERR_NO_SEARCH_TEXT,
	STATE_ERR_INVALID_HEX_LEN,
	STATE_ERR_MUST_BE_SAME_LEN,
	STATE_ERR_MUST_DIFFER,
	STATE_ERR_UNDO,
	STATE_ERR_DATA_VIEW
};

enum
{
	SR_STATE_NONE,
	SR_STATE_TEXT1,
	SR_STATE_TEXT2,
	SR_STATE_HEX1,
	SR_STATE_HEX2
};

struct st_flags
{
	unsigned use_colour      : 1;
	unsigned cur_hex_right   : 1;
	unsigned search_hex      : 1;
	unsigned search_ign_case : 1;
	unsigned search_wrapped  : 1;
	unsigned fixed_term_size : 1;
};

struct st_undo
{
	u_char *mem_pos;
	u_char prev_val;
	int cur_hex_right;
};

EXTERN struct termios saved_tio;
EXTERN struct stat file_stat;
EXTERN struct st_flags flags;
EXTERN struct st_undo undo_list[MAX_UNDO];
EXTERN off_t file_size;
EXTERN off_t file_malloc_size;
EXTERN int term_type;
EXTERN int term_height;
EXTERN int term_width;
EXTERN int term_div_y;
EXTERN int term_textbox_y;
EXTERN int term_pane_cols;
EXTERN int term_pane;
EXTERN int cursor_type;
EXTERN int cmd_x;
EXTERN int cmd_y;
EXTERN int cmd_state;
EXTERN int cmd_text_len;
EXTERN int sr_state;
EXTERN int sr_text_len;
EXTERN int sr_count;
EXTERN int search_text_len;
EXTERN int replace_text_len;
EXTERN int oldest_undo_pos;
EXTERN int next_undo_pos;
EXTERN int help_page;
EXTERN int decode_page;
EXTERN int undo_cnt;
EXTERN int total_updates;
EXTERN int total_inserts;
EXTERN int total_deletes;
EXTERN int total_undos;
EXTERN long goto_pos;
EXTERN u_char *mem_start;
EXTERN u_char *mem_end;
EXTERN u_char *mem_pane_start;
EXTERN u_char *mem_pane_end;
EXTERN u_char *mem_cursor;
EXTERN u_char *mem_search_find_start;
EXTERN u_char *mem_search_find_end;
EXTERN u_char *mem_undo_reset;
EXTERN u_char *mem_decode_view;
EXTERN u_char insert_char;
EXTERN u_char sr_text[CMD_TEXT_SIZE+1];
EXTERN u_char search_text[CMD_TEXT_SIZE+1];
EXTERN u_char replace_text[CMD_TEXT_SIZE+1];
EXTERN char cmd_text[CMD_TEXT_SIZE+1];
EXTERN char user_cmd;
EXTERN char substitute_char;
EXTERN char *filename;
EXTERN time_t esc_time;

/* keyboard.c */
void initKeyboard();
void readKeyboard();

/* signals.c */
void initSignals();

/* terminal.c */
void parseTerminalSize(char *str);
void getTermType();
void getTermSize();
void clearScreen();
void clearLine(int y);
void locate(int x, int y);
void setPaneStart(u_char *mem_pos);
void positionCursor();
void setCursorType(int type);
void scrollUp();
void scrollDown();
void pageUp();
void pageDown();

/* draw.c */
void drawScreen();
int  drawBanner(int line_flags);
void drawCmdPane();
void drawUndoList();
void drawDataView();

/* file.c */
void mapFile();
void setFileName(char *name);
int  saveFile(char *name);
void changeFileData(u_char c);
void insertAtCursorPos();
void deleteAtCursorPos();
void findText();
void doSearchAndReplace();

/* undo.c */
void initUndo();
void addUndo(u_char *ptr);
void undo();

/* printf.c */
void errprintf(const char *fmt, ...);
void syserrprintf(char *func);
void colprintf(const char *fmt, ...);

/* misc.c */
void clearCommandText();
void resetCommand();
void version();
void doExit(int code);
