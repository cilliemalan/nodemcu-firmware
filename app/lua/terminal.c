
#include "c_stdio.h"
#include "c_stdlib.h"
#include "c_string.h"
#include "user_interface.h"
#include "user_version.h"
#include "driver/readline.h"
#include "driver/uart.h"
#include "lua.h"


extern lua_Load gLoad;
extern bool uart0_echo;

static void echo_c(const char c) {
  if(uart0_echo) uart0_putc(c);
}

static void echo_s(const char* str) {
  if(uart0_echo) {
    while(str && *str) {
      uart0_putc(*(str++));
    }
  }
}



static bool noskipnl = false;
static char prevchar = 0;
static bool escaped = false;
static bool escape_control = false;
static int escape_parameter = 0;

// redefinitions for better modularity
#define line (gLoad.line)
#define line_end (gLoad.line_position)
#define line_size (gLoad.len)
#define line_hasdata (gLoad.done)
static int line_pos = 0;



static void terminate_line()
{

	//safety
	if (line_end >= (int)line_size)
	{
		line_end = line_size - 1;
	}

	//null terminate just in case
	line[line_end] = 0;

	//signal whether there is data
	line_hasdata = line_end > 0;

	//reset cursor to beginning
	line_pos = 0;
}

static void discard_line()
{
	//reset to beginning
	line_pos = 0;
	line_end = 0;
	line_hasdata = false;
	line[0] = 0;
}

static void reprint(int extraspaces)
{
	char* data = line + line_pos;
	int amt = line_end - line_pos;
	for (int i = 0; i < amt; i++)
	{
		echo_c(data[i]);
	}
	for (int i = 0; i < extraspaces; i++)
	{
		echo_c(' ');
	}
	for (int i = 0; i < amt + extraspaces; i++)
	{
		echo_c(0x08);
	}
}

static void eat(unsigned char c)
{
	if (line_end < (int)line_size)
	{
		if (line_pos != line_end)
		{
			for (int i = line_end; i > line_pos; i--)
			{
				line[i] = line[i - 1];
			}
		}

		line[line_pos] = c;
		++line_pos;
		++line_end;

		echo_c(c);
		if (line_pos != line_end)
		{
			reprint(0);
		}
	}
}

static void eatbackspace()
{
	if (line_pos > 0)
	{
		if (line_pos != line_end)
		{
			for (int i = line_pos; i < line_end; i++)
			{
				line[i - 1] = line[i];
			}
		}

		--line_pos;
		--line_end;
		echo_c(0x08);
		reprint(1);
	}
}

static void eatdelete()
{
	if (line_pos != line_end)
	{
		for (int i = line_pos; i < line_end - 1; i++)
		{
			line[i] = line[i + 1];
		}
	}
	--line_end;
	reprint(1);
}

static void cursor_up()
{
	//nothing
}

static void cursor_down()
{
	//nothing
}

static void cursor_left()
{
	if (line_pos > 0)
	{
		--line_pos;
		echo_c(0x08);
	}
}

static void cursor_right()
{
	if (line_pos < line_end)
	{
		echo_c(line[line_pos]);
		++line_pos;
	}
}

static void cursor_home()
{
	for (int i = 0; i < line_pos; i++) echo_c(0x08);
	line_pos = 0;
}

static void cursor_end()
{
	for (int i = line_pos; i < line_end; i++) echo_c(line[i]);
	line_pos = line_end;
}

static void leave_escape()
{
	escaped = false;
	escape_control = false;
	escape_parameter = 0;
}

int terminal_process_input(unsigned char c)
{
	bool line_done = false;

	//some checks for consistency and security
	if (line_end >= (int)line_size)
	{
		line_end = (int)line_size - 1;
	}
	if (line_pos > line_end)
	{
		line_pos = line_end;
	}


	bool eatchar = true;
	int result = 0;

	if (!escaped)
	{

		if (c < 0x20 || c == 0x7f)
		{
			eatchar = false;

			switch (c)
			{
			case 0x03: //ctrl+c
				discard_line();
				line_done = true;
				echo_c('\n');

				break;
			case '\r':
			case '\n':
				if (noskipnl ||
					(c == '\n' && prevchar != '\r') ||
					(c == '\r' && prevchar != '\n'))
				{
					terminate_line();
					line_done = true;
					echo_c('\n');
					noskipnl = false;
				}
				else
				{
					noskipnl = true;
				}

				break;
			case 0x7f: //backspace
			case 0x08: //backspace
				eatbackspace();
				break;
			case 0x1b: //control char
				escaped = true;
				break;
			}
		}
		else
		{
			eat(c);
		}
	}
	else
	{
		if (!escape_control)
		{
			switch (c)
			{
			case '[':
				escape_control = true;
				break;
			default:
				//invalid control char
				leave_escape();
			}
		}
		else
		{
			int number;
			switch (c)
			{
			case 'A':
				cursor_up();
				leave_escape();
				break;
			case 'B':
				cursor_down();
				leave_escape();
				break;
			case 'C':
				cursor_right();
				leave_escape();
				break;
			case 'D':
				cursor_left();
				leave_escape();
				break;
			case '0':
			case '1':
			case '2':
			case '3':
			case '4':
			case '5':
			case '6':
			case '7':
			case '8':
			case '9':
				number = c - '0';
				escape_parameter = escape_parameter * 10 + number;
				break;
			case '~':
				//home = 1
				//ins = 2
				//del = 3
				//end = 4
				//pgup 5
				//pdn 6
				switch (escape_parameter)
				{
				case 1:
					cursor_home();
					break;
				case 3:
					eatdelete();
					break;
				case 4:
					cursor_end();
					break;
				}
				leave_escape();
				break;
			default:
				//unkown escape control code
				leave_escape();
			}
		}
	}

	prevchar = c;

	return line_done;
}


