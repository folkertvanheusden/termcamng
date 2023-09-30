#include <cassert>
#include <cstring>
#include <stdlib.h>
#include <string>
#include <vector>

#include "logging.h"
#include "str.h"
#include "terminal.h"
#include "time.h"


terminal::terminal(font *const f, const int w, const int h, std::atomic_bool *const stop_flag) :
	f(f),
	w(w), h(h),
	stop_flag(stop_flag)
{
	screen = new pos_t[w * h]();

	for(int i=0; i<w * h; i++)
		screen[i].c = ' ';

	color_map[0][0] = {   0,   0,   0 };  // black
	color_map[0][1] = { 170,   0,   0 };  // red
	color_map[0][2] = {   0, 170,   0 };  // green
	color_map[0][3] = { 170,  85,   0 };  // yellow
	color_map[0][4] = {   0,   0, 170 };  // blue
	color_map[0][5] = { 170,   0, 170 };  // magenta
	color_map[0][6] = {   0, 170, 170 };  // cyan
	color_map[0][7] = { 170, 170, 170 };  // white
	color_map[1][0] = {  85,  85,  85 };  // bright black (gray)
	color_map[1][1] = { 255,  85,  85 };  // bright red
	color_map[1][2] = {  85, 255,  85 };  // bright green
	color_map[1][3] = { 255, 255,  85 };  // bright yellow
	color_map[1][4] = {  85,  85, 255 };  // bright blue
	color_map[1][5] = { 255,  85, 255 };  // bright magenta
	color_map[1][6] = {  85, 255, 255 };  // bright cyan
	color_map[1][7] = { 255, 255, 255 };  // bright white

	int i = 0;

	for(int b=0; b<2; b++) {
		for(int c=0; c<8; c++)
			color_map_256c[i++] = color_map[b][c];
	}

	// from xterm sources:
	color_map_256c[i++] = { 0x00, 0x00, 0x00 };
	color_map_256c[i++] = { 0x00, 0x00, 0x5f };
	color_map_256c[i++] = { 0x00, 0x00, 0x87 };
	color_map_256c[i++] = { 0x00, 0x00, 0xaf };
	color_map_256c[i++] = { 0x00, 0x00, 0xd7 };
	color_map_256c[i++] = { 0x00, 0x00, 0xff };
	color_map_256c[i++] = { 0x00, 0x5f, 0x00 };
	color_map_256c[i++] = { 0x00, 0x5f, 0x5f };
	color_map_256c[i++] = { 0x00, 0x5f, 0x87 };
	color_map_256c[i++] = { 0x00, 0x5f, 0xaf };
	color_map_256c[i++] = { 0x00, 0x5f, 0xd7 };
	color_map_256c[i++] = { 0x00, 0x5f, 0xff };
	color_map_256c[i++] = { 0x00, 0x87, 0x00 };
	color_map_256c[i++] = { 0x00, 0x87, 0x5f };
	color_map_256c[i++] = { 0x00, 0x87, 0x87 };
	color_map_256c[i++] = { 0x00, 0x87, 0xaf };
	color_map_256c[i++] = { 0x00, 0x87, 0xd7 };
	color_map_256c[i++] = { 0x00, 0x87, 0xff };
	color_map_256c[i++] = { 0x00, 0xaf, 0x00 };
	color_map_256c[i++] = { 0x00, 0xaf, 0x5f };
	color_map_256c[i++] = { 0x00, 0xaf, 0x87 };
	color_map_256c[i++] = { 0x00, 0xaf, 0xaf };
	color_map_256c[i++] = { 0x00, 0xaf, 0xd7 };
	color_map_256c[i++] = { 0x00, 0xaf, 0xff };
	color_map_256c[i++] = { 0x00, 0xd7, 0x00 };
	color_map_256c[i++] = { 0x00, 0xd7, 0x5f };
	color_map_256c[i++] = { 0x00, 0xd7, 0x87 };
	color_map_256c[i++] = { 0x00, 0xd7, 0xaf };
	color_map_256c[i++] = { 0x00, 0xd7, 0xd7 };
	color_map_256c[i++] = { 0x00, 0xd7, 0xff };
	color_map_256c[i++] = { 0x00, 0xff, 0x00 };
	color_map_256c[i++] = { 0x00, 0xff, 0x5f };
	color_map_256c[i++] = { 0x00, 0xff, 0x87 };
	color_map_256c[i++] = { 0x00, 0xff, 0xaf };
	color_map_256c[i++] = { 0x00, 0xff, 0xd7 };
	color_map_256c[i++] = { 0x00, 0xff, 0xff };
	color_map_256c[i++] = { 0x5f, 0x00, 0x00 };
	color_map_256c[i++] = { 0x5f, 0x00, 0x5f };
	color_map_256c[i++] = { 0x5f, 0x00, 0x87 };
	color_map_256c[i++] = { 0x5f, 0x00, 0xaf };
	color_map_256c[i++] = { 0x5f, 0x00, 0xd7 };
	color_map_256c[i++] = { 0x5f, 0x00, 0xff };
	color_map_256c[i++] = { 0x5f, 0x5f, 0x00 };
	color_map_256c[i++] = { 0x5f, 0x5f, 0x5f };
	color_map_256c[i++] = { 0x5f, 0x5f, 0x87 };
	color_map_256c[i++] = { 0x5f, 0x5f, 0xaf };
	color_map_256c[i++] = { 0x5f, 0x5f, 0xd7 };
	color_map_256c[i++] = { 0x5f, 0x5f, 0xff };
	color_map_256c[i++] = { 0x5f, 0x87, 0x00 };
	color_map_256c[i++] = { 0x5f, 0x87, 0x5f };
	color_map_256c[i++] = { 0x5f, 0x87, 0x87 };
	color_map_256c[i++] = { 0x5f, 0x87, 0xaf };
	color_map_256c[i++] = { 0x5f, 0x87, 0xd7 };
	color_map_256c[i++] = { 0x5f, 0x87, 0xff };
	color_map_256c[i++] = { 0x5f, 0xaf, 0x00 };
	color_map_256c[i++] = { 0x5f, 0xaf, 0x5f };
	color_map_256c[i++] = { 0x5f, 0xaf, 0x87 };
	color_map_256c[i++] = { 0x5f, 0xaf, 0xaf };
	color_map_256c[i++] = { 0x5f, 0xaf, 0xd7 };
	color_map_256c[i++] = { 0x5f, 0xaf, 0xff };
	color_map_256c[i++] = { 0x5f, 0xd7, 0x00 };
	color_map_256c[i++] = { 0x5f, 0xd7, 0x5f };
	color_map_256c[i++] = { 0x5f, 0xd7, 0x87 };
	color_map_256c[i++] = { 0x5f, 0xd7, 0xaf };
	color_map_256c[i++] = { 0x5f, 0xd7, 0xd7 };
	color_map_256c[i++] = { 0x5f, 0xd7, 0xff };
	color_map_256c[i++] = { 0x5f, 0xff, 0x00 };
	color_map_256c[i++] = { 0x5f, 0xff, 0x5f };
	color_map_256c[i++] = { 0x5f, 0xff, 0x87 };
	color_map_256c[i++] = { 0x5f, 0xff, 0xaf };
	color_map_256c[i++] = { 0x5f, 0xff, 0xd7 };
	color_map_256c[i++] = { 0x5f, 0xff, 0xff };
	color_map_256c[i++] = { 0x87, 0x00, 0x00 };
	color_map_256c[i++] = { 0x87, 0x00, 0x5f };
	color_map_256c[i++] = { 0x87, 0x00, 0x87 };
	color_map_256c[i++] = { 0x87, 0x00, 0xaf };
	color_map_256c[i++] = { 0x87, 0x00, 0xd7 };
	color_map_256c[i++] = { 0x87, 0x00, 0xff };
	color_map_256c[i++] = { 0x87, 0x5f, 0x00 };
	color_map_256c[i++] = { 0x87, 0x5f, 0x5f };
	color_map_256c[i++] = { 0x87, 0x5f, 0x87 };
	color_map_256c[i++] = { 0x87, 0x5f, 0xaf };
	color_map_256c[i++] = { 0x87, 0x5f, 0xd7 };
	color_map_256c[i++] = { 0x87, 0x5f, 0xff };
	color_map_256c[i++] = { 0x87, 0x87, 0x00 };
	color_map_256c[i++] = { 0x87, 0x87, 0x5f };
	color_map_256c[i++] = { 0x87, 0x87, 0x87 };
	color_map_256c[i++] = { 0x87, 0x87, 0xaf };
	color_map_256c[i++] = { 0x87, 0x87, 0xd7 };
	color_map_256c[i++] = { 0x87, 0x87, 0xff };
	color_map_256c[i++] = { 0x87, 0xaf, 0x00 };
	color_map_256c[i++] = { 0x87, 0xaf, 0x5f };
	color_map_256c[i++] = { 0x87, 0xaf, 0x87 };
	color_map_256c[i++] = { 0x87, 0xaf, 0xaf };
	color_map_256c[i++] = { 0x87, 0xaf, 0xd7 };
	color_map_256c[i++] = { 0x87, 0xaf, 0xff };
	color_map_256c[i++] = { 0x87, 0xd7, 0x00 };
	color_map_256c[i++] = { 0x87, 0xd7, 0x5f };
	color_map_256c[i++] = { 0x87, 0xd7, 0x87 };
	color_map_256c[i++] = { 0x87, 0xd7, 0xaf };
	color_map_256c[i++] = { 0x87, 0xd7, 0xd7 };
	color_map_256c[i++] = { 0x87, 0xd7, 0xff };
	color_map_256c[i++] = { 0x87, 0xff, 0x00 };
	color_map_256c[i++] = { 0x87, 0xff, 0x5f };
	color_map_256c[i++] = { 0x87, 0xff, 0x87 };
	color_map_256c[i++] = { 0x87, 0xff, 0xaf };
	color_map_256c[i++] = { 0x87, 0xff, 0xd7 };
	color_map_256c[i++] = { 0x87, 0xff, 0xff };
	color_map_256c[i++] = { 0xaf, 0x00, 0x00 };
	color_map_256c[i++] = { 0xaf, 0x00, 0x5f };
	color_map_256c[i++] = { 0xaf, 0x00, 0x87 };
	color_map_256c[i++] = { 0xaf, 0x00, 0xaf };
	color_map_256c[i++] = { 0xaf, 0x00, 0xd7 };
	color_map_256c[i++] = { 0xaf, 0x00, 0xff };
	color_map_256c[i++] = { 0xaf, 0x5f, 0x00 };
	color_map_256c[i++] = { 0xaf, 0x5f, 0x5f };
	color_map_256c[i++] = { 0xaf, 0x5f, 0x87 };
	color_map_256c[i++] = { 0xaf, 0x5f, 0xaf };
	color_map_256c[i++] = { 0xaf, 0x5f, 0xd7 };
	color_map_256c[i++] = { 0xaf, 0x5f, 0xff };
	color_map_256c[i++] = { 0xaf, 0x87, 0x00 };
	color_map_256c[i++] = { 0xaf, 0x87, 0x5f };
	color_map_256c[i++] = { 0xaf, 0x87, 0x87 };
	color_map_256c[i++] = { 0xaf, 0x87, 0xaf };
	color_map_256c[i++] = { 0xaf, 0x87, 0xd7 };
	color_map_256c[i++] = { 0xaf, 0x87, 0xff };
	color_map_256c[i++] = { 0xaf, 0xaf, 0x00 };
	color_map_256c[i++] = { 0xaf, 0xaf, 0x5f };
	color_map_256c[i++] = { 0xaf, 0xaf, 0x87 };
	color_map_256c[i++] = { 0xaf, 0xaf, 0xaf };
	color_map_256c[i++] = { 0xaf, 0xaf, 0xd7 };
	color_map_256c[i++] = { 0xaf, 0xaf, 0xff };
	color_map_256c[i++] = { 0xaf, 0xd7, 0x00 };
	color_map_256c[i++] = { 0xaf, 0xd7, 0x5f };
	color_map_256c[i++] = { 0xaf, 0xd7, 0x87 };
	color_map_256c[i++] = { 0xaf, 0xd7, 0xaf };
	color_map_256c[i++] = { 0xaf, 0xd7, 0xd7 };
	color_map_256c[i++] = { 0xaf, 0xd7, 0xff };
	color_map_256c[i++] = { 0xaf, 0xff, 0x00 };
	color_map_256c[i++] = { 0xaf, 0xff, 0x5f };
	color_map_256c[i++] = { 0xaf, 0xff, 0x87 };
	color_map_256c[i++] = { 0xaf, 0xff, 0xaf };
	color_map_256c[i++] = { 0xaf, 0xff, 0xd7 };
	color_map_256c[i++] = { 0xaf, 0xff, 0xff };
	color_map_256c[i++] = { 0xd7, 0x00, 0x00 };
	color_map_256c[i++] = { 0xd7, 0x00, 0x5f };
	color_map_256c[i++] = { 0xd7, 0x00, 0x87 };
	color_map_256c[i++] = { 0xd7, 0x00, 0xaf };
	color_map_256c[i++] = { 0xd7, 0x00, 0xd7 };
	color_map_256c[i++] = { 0xd7, 0x00, 0xff };
	color_map_256c[i++] = { 0xd7, 0x5f, 0x00 };
	color_map_256c[i++] = { 0xd7, 0x5f, 0x5f };
	color_map_256c[i++] = { 0xd7, 0x5f, 0x87 };
	color_map_256c[i++] = { 0xd7, 0x5f, 0xaf };
	color_map_256c[i++] = { 0xd7, 0x5f, 0xd7 };
	color_map_256c[i++] = { 0xd7, 0x5f, 0xff };
	color_map_256c[i++] = { 0xd7, 0x87, 0x00 };
	color_map_256c[i++] = { 0xd7, 0x87, 0x5f };
	color_map_256c[i++] = { 0xd7, 0x87, 0x87 };
	color_map_256c[i++] = { 0xd7, 0x87, 0xaf };
	color_map_256c[i++] = { 0xd7, 0x87, 0xd7 };
	color_map_256c[i++] = { 0xd7, 0x87, 0xff };
	color_map_256c[i++] = { 0xd7, 0xaf, 0x00 };
	color_map_256c[i++] = { 0xd7, 0xaf, 0x5f };
	color_map_256c[i++] = { 0xd7, 0xaf, 0x87 };
	color_map_256c[i++] = { 0xd7, 0xaf, 0xaf };
	color_map_256c[i++] = { 0xd7, 0xaf, 0xd7 };
	color_map_256c[i++] = { 0xd7, 0xaf, 0xff };
	color_map_256c[i++] = { 0xd7, 0xd7, 0x00 };
	color_map_256c[i++] = { 0xd7, 0xd7, 0x5f };
	color_map_256c[i++] = { 0xd7, 0xd7, 0x87 };
	color_map_256c[i++] = { 0xd7, 0xd7, 0xaf };
	color_map_256c[i++] = { 0xd7, 0xd7, 0xd7 };
	color_map_256c[i++] = { 0xd7, 0xd7, 0xff };
	color_map_256c[i++] = { 0xd7, 0xff, 0x00 };
	color_map_256c[i++] = { 0xd7, 0xff, 0x5f };
	color_map_256c[i++] = { 0xd7, 0xff, 0x87 };
	color_map_256c[i++] = { 0xd7, 0xff, 0xaf };
	color_map_256c[i++] = { 0xd7, 0xff, 0xd7 };
	color_map_256c[i++] = { 0xd7, 0xff, 0xff };
	color_map_256c[i++] = { 0xff, 0x00, 0x00 };
	color_map_256c[i++] = { 0xff, 0x00, 0x5f };
	color_map_256c[i++] = { 0xff, 0x00, 0x87 };
	color_map_256c[i++] = { 0xff, 0x00, 0xaf };
	color_map_256c[i++] = { 0xff, 0x00, 0xd7 };
	color_map_256c[i++] = { 0xff, 0x00, 0xff };
	color_map_256c[i++] = { 0xff, 0x5f, 0x00 };
	color_map_256c[i++] = { 0xff, 0x5f, 0x5f };
	color_map_256c[i++] = { 0xff, 0x5f, 0x87 };
	color_map_256c[i++] = { 0xff, 0x5f, 0xaf };
	color_map_256c[i++] = { 0xff, 0x5f, 0xd7 };
	color_map_256c[i++] = { 0xff, 0x5f, 0xff };
	color_map_256c[i++] = { 0xff, 0x87, 0x00 };
	color_map_256c[i++] = { 0xff, 0x87, 0x5f };
	color_map_256c[i++] = { 0xff, 0x87, 0x87 };
	color_map_256c[i++] = { 0xff, 0x87, 0xaf };
	color_map_256c[i++] = { 0xff, 0x87, 0xd7 };
	color_map_256c[i++] = { 0xff, 0x87, 0xff };
	color_map_256c[i++] = { 0xff, 0xaf, 0x00 };
	color_map_256c[i++] = { 0xff, 0xaf, 0x5f };
	color_map_256c[i++] = { 0xff, 0xaf, 0x87 };
	color_map_256c[i++] = { 0xff, 0xaf, 0xaf };
	color_map_256c[i++] = { 0xff, 0xaf, 0xd7 };
	color_map_256c[i++] = { 0xff, 0xaf, 0xff };
	color_map_256c[i++] = { 0xff, 0xd7, 0x00 };
	color_map_256c[i++] = { 0xff, 0xd7, 0x5f };
	color_map_256c[i++] = { 0xff, 0xd7, 0x87 };
	color_map_256c[i++] = { 0xff, 0xd7, 0xaf };
	color_map_256c[i++] = { 0xff, 0xd7, 0xd7 };
	color_map_256c[i++] = { 0xff, 0xd7, 0xff };
	color_map_256c[i++] = { 0xff, 0xff, 0x00 };
	color_map_256c[i++] = { 0xff, 0xff, 0x5f };
	color_map_256c[i++] = { 0xff, 0xff, 0x87 };
	color_map_256c[i++] = { 0xff, 0xff, 0xaf };
	color_map_256c[i++] = { 0xff, 0xff, 0xd7 };
	color_map_256c[i++] = { 0xff, 0xff, 0xff };
	color_map_256c[i++] = { 0x08, 0x08, 0x08 };
	color_map_256c[i++] = { 0x12, 0x12, 0x12 };
	color_map_256c[i++] = { 0x1c, 0x1c, 0x1c };
	color_map_256c[i++] = { 0x26, 0x26, 0x26 };
	color_map_256c[i++] = { 0x30, 0x30, 0x30 };
	color_map_256c[i++] = { 0x3a, 0x3a, 0x3a };
	color_map_256c[i++] = { 0x44, 0x44, 0x44 };
	color_map_256c[i++] = { 0x4e, 0x4e, 0x4e };
	color_map_256c[i++] = { 0x58, 0x58, 0x58 };
	color_map_256c[i++] = { 0x62, 0x62, 0x62 };
	color_map_256c[i++] = { 0x6c, 0x6c, 0x6c };
	color_map_256c[i++] = { 0x76, 0x76, 0x76 };
	color_map_256c[i++] = { 0x80, 0x80, 0x80 };
	color_map_256c[i++] = { 0x8a, 0x8a, 0x8a };
	color_map_256c[i++] = { 0x94, 0x94, 0x94 };
	color_map_256c[i++] = { 0x9e, 0x9e, 0x9e };
	color_map_256c[i++] = { 0xa8, 0xa8, 0xa8 };
	color_map_256c[i++] = { 0xb2, 0xb2, 0xb2 };
	color_map_256c[i++] = { 0xbc, 0xbc, 0xbc };
	color_map_256c[i++] = { 0xc6, 0xc6, 0xc6 };
	color_map_256c[i++] = { 0xd0, 0xd0, 0xd0 };
	color_map_256c[i++] = { 0xda, 0xda, 0xda };
	color_map_256c[i++] = { 0xe4, 0xe4, 0xe4 };
	color_map_256c[i++] = { 0xee, 0xee, 0xee };

	assert(i == 256);
}

terminal::~terminal()
{
	delete [] screen;
}

std::pair<int, int> terminal::get_current_xy()
{
	return { x, y };
}

void terminal::delete_line(const int y)
{
	int offset_to   = y * w;
	int offset_from = (y + 1) * w;

	int n_characters_to_move = w * h - offset_from;

	if (n_characters_to_move == 0)
		erase_line(y);
	else {
		memmove(&screen[offset_to], &screen[offset_from], n_characters_to_move * sizeof(screen[0]));

		erase_line(h - 1);
	}
}

void terminal::insert_line(const int y)
{
	int offset_to    = (y + 1) * w;
	int offset_from  = y * w;

	int n_characters_to_move = w * h - offset_to;

	if (n_characters_to_move > 0)
		memmove(&screen[offset_to], &screen[offset_from], n_characters_to_move * sizeof(screen[0]));

	erase_line(y);
}

void terminal::insert_character(const int n)
{
	int n_left      = w - x - 1;
	int offset_from = y * w + x;
	int offset_to   = y * w + x + 1;

	for(int i=0; i<n; i++) {
		memmove(&screen[offset_to], &screen[offset_from], n_left * sizeof(screen[0]));

		erase_cell(x, y);
	}
}

void terminal::delete_character(const int n)
{
	int n_left      = w - x;
	int offset_to   = y * w + x;
	int offset_from = y * w + x + 1;

	for(int i=0; i<n; i++) {
		memmove(&screen[offset_to], &screen[offset_from], n_left * sizeof(screen[0]));

		erase_cell(w - 1, y);
	}
}

int evaluate_n(const std::optional<int> & in)
{
	if (in.has_value()) {
		if (in.value() == 0)
			return 1;

		return in.value();
	}

	return 1;
}

void terminal::emit_character(const uint32_t c)
{
	if (x >= w) {
		if (wraparound) {
			x = 0;

			y++;
		}
		else {
			x = w - 1;  // will be put back to w below
		}
	}

	screen[y * w + x].c           = c;
	screen[y * w + x].fg_col_ansi = fg_col_ansi;
	screen[y * w + x].fg_rgb      = fg_rgb;
	screen[y * w + x].bg_col_ansi = bg_col_ansi;
	screen[y * w + x].bg_rgb      = bg_rgb;
	screen[y * w + x].attr        = attr;

	x++;

	last_character = c;
}

std::optional<std::string> terminal::process_escape(const char cmd, const std::string & parameters, const bool is_short)
{
	std::optional<std::string> send_back;

	std::vector<std::string> pars = split(parameters, ";");

	std::optional<int> par1;
	std::optional<int> par2;

	if (pars.size() >= 1)
		par1 = std::atoi(pars[0].c_str());

	if (pars.size() >= 2)
		par2 = std::atoi(pars[1].c_str());

	if (cmd == 'A') {  // cursor up
		y -= evaluate_n(par1);

		if (y < 0) {
			dolog(ll_info, "%c: y=%d", cmd, y);
			y = 0;
		}
	}
	else if (cmd == 'B') {  // cursor down
		y += evaluate_n(par1);

		if (y >= h) {
			dolog(ll_info, "%c: y=%d", cmd, y);
			y = h - 1;
		}
	}
	else if (cmd == 'b') { // repeat
		int n = evaluate_n(par1);

		for(int i=0; i<n; i++)
			emit_character(last_character);
	}
	else if (cmd == 'C') {  // cursor forward  CUF
		x += evaluate_n(par1);

		if (x >= w) {
			dolog(ll_info, "%c: x=%d", cmd, x);
			x = w - 1;
		}
	}
	else if (cmd == 'D') {  // cursor backward  CUB
		x -= evaluate_n(par1);

		if (x < 0) {
			dolog(ll_info, "%c: x=%d", cmd, x);
			x = 0;
		}
	}
	else if (cmd == 'd') {  // set y(?)
		y = par1.has_value() ? par1.value() - 1 : 0;

		if (y < 0) {
			dolog(ll_info, "%c: y=%d", cmd, y);
			y = 0;
		}
		else if (y >= h) {
			dolog(ll_info, "%c: y=%d", cmd, y);
			y = h - 1;
		}
	}
	else if (cmd == 'E') {  // Move cursor to the beginning of the line n lines down
		y += evaluate_n(par1);

		if (y >= h) {
			dolog(ll_info, "%c: y=%d", cmd, y);
			y = h - 1;
		}

		x = 0;
	}
	else if (cmd == 'G') {  // cursor horizontal absolute
		x = par1.has_value() ? par1.value() - 1 : 0;

		if (x < 0) {
			dolog(ll_info, "%c: x=%d", cmd, x);
			x = 0;
		}
		else if (x >= w) {
			dolog(ll_info, "%c: x=%d", cmd, x);
			x = w - 1;
		}
	}
	else if (cmd == 'H' || cmd == 'f') {  // set position  CUP
		y = par1.has_value() ? par1.value() - 1 : 0;

		if (y < 0) {
			dolog(ll_info, "%c: y=%d", cmd, y);
			y = 0;
		}
		else if (y >= h) {
			dolog(ll_info, "%c: y=%d", cmd, y);
			y = h - 1;
		}

		x = par2.has_value() ? par2.value() - 1 : 0;

		if (x < 0) {
			dolog(ll_info, "%c: x=%d", cmd, x);
			x = 0;
		}
		else if (x >= w) {
			dolog(ll_info, "%c: x=%d", cmd, x);
			x = w - 1;
		}
	}
	else if (cmd == 'J') {
		int val = par1.has_value() ? par1.value() : 0;

		int start_pos = 0;
		int end_pos   = y * w + x;

		if (val == 0)
			start_pos = y * w + x, end_pos = w * h;
		else if (val == 1)
			end_pos++;
		else if (val == 2 || val == 3) {
			end_pos = w * h;

			x = y = 0;
		}
		else {
			dolog(ll_info, "CSI %c %d not supported", cmd, val);
		}

		for(int pos=start_pos; pos<end_pos; pos++) {
			screen[pos].c           = ' ';
			screen[pos].fg_col_ansi = fg_col_ansi;
			screen[pos].fg_rgb      = fg_rgb;
			screen[pos].bg_col_ansi = bg_col_ansi;
			screen[pos].bg_rgb      = bg_rgb;
			screen[pos].attr        = attr;
		}
	}
	else if (cmd == 'K') {
		int val = par1.has_value() ? par1.value() : 0;

		int start_x = 0;
		int end_x   = w;

		if (val == 0)
			start_x = x;
		else if (val == 1)
			end_x   = x + 1;
		else if (val == 2) {
			// use defaults
		}
		else {
			dolog(ll_info, "CSI %c %d not supported", cmd, val);
		}

		for(int cx=start_x; cx<end_x; cx++)
			erase_cell(cx, y);
	}
	else if (cmd == 'L') {
		int n = evaluate_n(par1);

		for(int i=0; i<n; i++)
			insert_line(y);

		x = 0;
	}
	else if (cmd == 'h') {
		if (parameters == "?7")
			wraparound = true;
		else
			dolog(ll_info, "%s %c not supported", parameters.c_str(), cmd);
	}
	else if (cmd == 'l') {
		if (parameters == "?7")
			wraparound = false;
		else
			dolog(ll_info, "%s %c not supported", parameters.c_str(), cmd);
	}
	else if (cmd == 'M') {
		int n = evaluate_n(par1);

		if (is_short) {
			for(int i=0; i<n; i++) {
				y--;

				if (y < 0) {
					y = 0;

					insert_line(0);
				}
			}
		}
		else {
			for(int i=0; i<n; i++)
				delete_line(y);

			x = 0;
		}
	}
	else if (cmd == 'm') {
		if (pars.empty())
			fg_col_ansi = 7, bg_col_ansi = 0, attr = 0;
		else {
			bool fg_is_rgb = true;
			int rgb_fg_index = -1;
			bool is_fg = true;
			bool bg_is_rgb = true;
			int rgb_bg_index = -1;

			for(size_t i=0; i<pars.size(); i++) {
				int par_val = std::atoi(pars.at(i).c_str());

				if (par_val >= 30 && par_val <= 37)  // fg color
					fg_col_ansi = par_val - 30;
				else if (par_val == 38) {
					fg_col_ansi = -1;  // rgb
					is_fg = true;
				}
				else if (par_val == 39)
					fg_col_ansi = 7;
				else if (par_val == 48) {
					bg_col_ansi = -1;  // rgb
					is_fg = false;
				}
				else if (par_val == 49)
					bg_col_ansi = 0;
				else if (par_val >= 40 && par_val <= 47)  // bg color
					bg_col_ansi = par_val - 40;
				else if (par_val == 0)  // reset
					fg_col_ansi = bg_col_ansi = attr = 0;
				else if (par_val == 1)  // bold
					attr |= A_BOLD;
				else if (par_val == 22)  // bold off
					attr &= ~A_BOLD;
				else if (par_val == 2) {  // faint or rgb selection
					if (fg_col_ansi == -1 && is_fg == true)
						rgb_fg_index = i + 1, i += 3, fg_is_rgb = true;
					else if (bg_col_ansi == -1 && is_fg == false)
						rgb_bg_index = i + 1, i += 3, bg_is_rgb = true;
					else if (fg_col_ansi != -1 && bg_col_ansi != -1)
						attr |= A_DIM;
					else
						dolog(ll_info, "rgb selection failed (%d,%d / %d)", fg_col_ansi, bg_col_ansi, is_fg);
				}
				else if (par_val == 3)  // italic on
					attr |= A_ITALIC;
				else if (par_val == 23)  // italic off
					attr &= ~A_ITALIC;
				else if (par_val == 4)  // underline on
					attr |= A_UNDERLINE;
				else if (par_val == 24)  // underline off
					attr &= ~A_UNDERLINE;
				else if (par_val == 5) {  // 256 color mode / blink on
					if (fg_col_ansi == -1 && is_fg == true)
						rgb_fg_index = i + 1, i += 1, fg_is_rgb = false;
					else if (bg_col_ansi == -1 && is_fg == false)
						rgb_bg_index = i + 1, i += 1, bg_is_rgb = false;
					else
						attr |= A_BLINK;
				}
				else if (par_val == 6)  // (rapid) blink on
					attr |= A_BLINK;
				else if (par_val == 25)  // blink off
					attr &= ~A_BLINK;
				else if (par_val == 7)  // inverse video on
					attr ^= A_INVERSE;
				else if (par_val == 27)  // inverse video off
					attr &= ~A_INVERSE;
				else if (par_val == 9)  // strikethrough on
					attr |= A_STRIKETHROUGH;
				else if (par_val == 29)  // strikethrough off
					attr &= ~A_STRIKETHROUGH;
				else if (par_val >= 10 && par_val <= 19) {
					// font selection
				}
				else if (par_val >= 90 && par_val <= 97) {  // fg color bright
					fg_col_ansi = par_val - 90;
					attr |= A_BOLD;
				}
				else if (par_val >= 100 && par_val <= 107) {  // bg color bright
					bg_col_ansi = par_val - 100;
					attr |= A_BOLD;
				}
				else {
					dolog(ll_info, "code %d for 'm' not supported", par_val);
				}
			}

			if (rgb_fg_index != -1 && fg_is_rgb == true && pars.size() - rgb_fg_index >= 3) {
				fg_rgb.r = std::atoi(pars.at(rgb_fg_index + 0).c_str());
				fg_rgb.g = std::atoi(pars.at(rgb_fg_index + 1).c_str());
				fg_rgb.b = std::atoi(pars.at(rgb_fg_index + 2).c_str());
			}
			if (rgb_fg_index != -1 && fg_is_rgb == false && pars.size() - rgb_fg_index >= 1) {
				fg_rgb = color_map_256c[std::atoi(pars.at(rgb_fg_index).c_str())];
			}

			if (rgb_bg_index != -1 && pars.size() - rgb_bg_index >= 3) {
				bg_rgb.r = std::atoi(pars.at(rgb_bg_index + 0).c_str());
				bg_rgb.g = std::atoi(pars.at(rgb_bg_index + 1).c_str());
				bg_rgb.b = std::atoi(pars.at(rgb_bg_index + 2).c_str());
			}
			if (rgb_bg_index != -1 && bg_is_rgb == false && pars.size() - rgb_bg_index >= 1) {
				bg_rgb = color_map_256c[std::atoi(pars.at(rgb_bg_index).c_str())];
			}
		}
	}
	else if (cmd == 'n') {  // device status report (DSR)
		if (par1 == 5)  // status report
			send_back = "\033[0n";  // OK
		else if (par1 == 6)  // report cursor position (CPR) [row;column]
			send_back = myformat("\033[%d;%dR", y + 1, x + 1);
		else {
			dolog(ll_info, "code %d for 'n' not supported", par1);
		}
	}
	else if (cmd == 'c') {  // "what are you"
		send_back = "\033[?1;0c";
	}
	else if (cmd == 'X') {  // erase character
		int offset = y * w + x;

		if (par1 == 0)
			par1 = 1;

		for(int i=0; i<par1; i++) {
			screen[offset + i].c           = ' ';
			screen[offset + i].fg_col_ansi = fg_col_ansi;
			screen[offset + i].fg_rgb      = fg_rgb;
			screen[offset + i].bg_col_ansi = bg_col_ansi;
			screen[offset + i].bg_rgb      = bg_rgb;
			screen[offset + i].attr        = attr;
		}
	}
	else if (cmd == 'P') {  // delete character
		int n = evaluate_n(par1);

		delete_character(n);
	}
	else if (cmd == '@') {  // insert character
		int n = evaluate_n(par1);

		insert_character(n);
	}
	else if (cmd == ']') {  // "operating system command", terminated with ST (ESC \)
		OSC = true;
	}
	else {
		dolog(ll_info, "Escape ^[[ %s %c not supported", parameters.c_str(), cmd);

		send_back = myformat("%c", cmd);
	}

	return send_back;
}

std::optional<std::string> terminal::process_escape(const char cmd, const char G)
{
	if (cmd == 'A' || cmd == 'B' || cmd == '0' || cmd == '1' || cmd == '2')
		return { };

	dolog(ll_info, "Escape %c %c not supported", G, cmd);

	return { };
}

std::optional<std::string> terminal::process_input(const char *const in, const size_t len)
{
	std::optional<std::string> send_back;

	for(size_t i=0; i<len; i++) {
		if (in[i] == 13)  // carriage return
			x = 0, utf8_len = 0;
		else if (in[i] == 10)  // new line
			y++, utf8_len = 0;
		else if (in[i] == 8) {  // backspace
			if (x)
				x--;
			else if (y)
				x = 0, y--;

			utf8_len = 0;
		}
		else if (in[i] == 9) {  // tab
			x &= ~7;
			x += 8;

			if (x >= w)
				x -= w, y++;

			utf8_len = 0;
		}
		// ANSI escape handling
		else if (in[i] == 27 && escape_state == E_NONE)
			escape_state = E_ESC, utf8_len = 0;
		else if (OSC) {
			// < 32: any other C1 character can terminate a sequence (https://vt100.net/docs/vt510-rm/chapter4.html)
			if ((escape_state == E_ESC && in[i] == '\\') || in[i] < 32){
				OSC = false;
				escape_state = E_NONE;
			}
		}
		else if (in[i] == '[' && escape_state == E_ESC)
			escape_state = E_SQ_BRACKET, utf8_len = 0;
		else if (in[i] == '(' && escape_state == E_ESC)
			escape_state = E_R1_BRACKET, utf8_len = 0;
		else if (in[i] == ')' && escape_state == E_ESC)
			escape_state = E_R2_BRACKET, utf8_len = 0;
		else if (escape_state == E_R1_BRACKET) {
			process_escape(in[i], '(');

			escape_state = E_NONE;
			escape_value.clear();
		}
		else if (escape_state == E_R2_BRACKET) {
			process_escape(in[i], ')');

			escape_state = E_NONE;
			escape_value.clear();
		}
		else if (escape_state == E_SQ_BRACKET || escape_state == E_VALUES || escape_state == E_ESC) {
			if ((in[i] >= '0' && in[i] <= '9') || in[i] == ';' || in[i] == '?') {
				if (escape_state == E_SQ_BRACKET)
					escape_state = E_VALUES;

				escape_value += in[i];
			}
			else if (in[i] >= 0x40 && in[i] <= 0x7e) {
				// because of 'b'
				std::string temp        = escape_value;
				bool        e_state_esc = escape_state == E_ESC;

				escape_state = E_NONE;
				escape_value.clear();

				send_back = process_escape(in[i], temp, e_state_esc);
			}
			else {
				dolog(ll_info, "escape [%s%c not supported", escape_value.c_str(), in[i]);

				escape_state = E_NONE;
				escape_value.clear();
			}

			utf8_len = 0;
		}
		// "regular" text
		else {
			uint32_t c = uint32_t(-1);

			if (utf8_len) {
				utf8_code <<= 6;
				utf8_code |= in[i] & 63;

				utf8_len--;

				if (utf8_len == 0)
					c = utf8_code;
			}
			else if ((in[i] & 0xe0) == 0xc0) {
				utf8_code = in[i] & 31;
				utf8_len = 1;
			}
			else if ((in[i] & 0xf0) == 0xe0) {
				utf8_code = in[i] & 15;
				utf8_len = 2;
			}
			else if ((in[i] & 0xf8) == 0xf0) {
				utf8_code = in[i] & 7;
				utf8_len = 3;
			}
			else {
				c = in[i];
			}

			if (c != uint32_t(-1))
				emit_character(c);
		}

		if (y == h) {
			delete_line(0);
			
			y = h - 1;
		}
	}

	std::unique_lock<std::mutex> lck(lock);

	latest_update = get_ms();

	cond.notify_all();

	return send_back;
}

std::optional<std::string> terminal::process_input(const std::string & in)
{
	return process_input(in.c_str(), in.size());
}

void terminal::render(uint64_t *const ts_after, const int max_wait, uint8_t **const out, int *const out_w, int *const out_h)
{
	uint64_t start_wait = get_ms();

	std::unique_lock<std::mutex> lck(lock);

	while(latest_update <= *ts_after && !*stop_flag && (get_ms() - start_wait < uint64_t(max_wait) || max_wait <= 0)) {
		int wait_for_delay = 500;

		if (max_wait > 0) {
			int interval_time_left = std::max(0, max_wait - int(get_ms() - start_wait));

			wait_for_delay = std::min(interval_time_left, std::min(max_wait, 500));
		}

		if (wait_for_delay > 0)
			cond.wait_for(lck, std::chrono::milliseconds(wait_for_delay));
	}

	*ts_after = latest_update;

	if (start_wait - blink_switch_ts >= 60000 / 150) {
		blink_state     = !blink_state;
		blink_switch_ts = latest_update;
	}

	const int char_w    = f->get_width();
	const int char_h    = f->get_height();

	int pixels_per_row  = w * char_w;

	*out = reinterpret_cast<uint8_t *>(calloc(1, w * char_w * h * char_h * 3));

	*out_w = pixels_per_row;
	*out_h = h * char_h;

	for(int cy=0; cy<h; cy++) {
		for(int cx=0; cx<w; cx++) {
			int      offset       = cy * w + cx;
			uint32_t c            = screen[offset].c;

			int      bold         = !!(screen[offset].attr & A_BOLD);
			int      dim          = !!(screen[offset].attr & A_DIM);

			font::intensity_t intensity = font::intensity_t::I_NORMAL;

			if (bold && !dim)
				intensity = font::intensity_t::I_BOLD;
			else if (dim)
				intensity = font::intensity_t::I_DIM;

			int      fg_color     = screen[offset].fg_col_ansi;
			int      bg_color     = screen[offset].bg_col_ansi;

			if (fg_color == bg_color && fg_color != -1)
				fg_color = 7, bg_color = 0;

			rgb_t    fg;
			if (fg_color == -1)
				fg   = screen[offset].fg_rgb;
			else
				fg   = color_map[bold][fg_color];

			rgb_t    bg;
			if (bg_color == -1)
				bg   = screen[offset].bg_rgb;
			else
				bg   = color_map[0][bg_color];

			bool     inverse      = !!(screen[offset].attr & A_INVERSE);

			bool     blink        = !!(screen[offset].attr & A_BLINK);

			if (blink)
				inverse = blink_state;

			bool     strikethrough= !!(screen[offset].attr & A_STRIKETHROUGH);

			bool     underline    = !!(screen[offset].attr & A_UNDERLINE);

			int     x            = cx * char_w;
			int     y            = cy * char_h;

			// TODO: italic (A_ITALIC)

			if (!f->draw_glyph(c, char_h, intensity, inverse, underline, strikethrough, fg, bg, x, y, *out, *out_w, *out_h)) {
				// TODO
			}
		}
	}
}

char terminal::get_char_at(const int cx, const int cy) const
{
	int offset = cy * w + cx;

	return screen[offset].c;
}

pos_t terminal::get_cell_at(const int cx, const int cy) const
{
	int offset = cy * w + cx;

	return screen[offset];
}

void terminal::erase_cell(const int cx, const int cy)
{
	const int offset = cy * w + cx;

	screen[offset].c           = ' ';
	screen[offset].fg_col_ansi = fg_col_ansi;
	screen[offset].bg_col_ansi = bg_col_ansi;
	screen[offset].attr        = attr;
}

void terminal::erase_line(const int cy)
{
	for(int cx=0; cx<w; cx++)
		erase_cell(cx, cy);
}
