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

	if (n_characters_to_move == 0) {
		for(int cx = 0; cx<w; cx++)
			screen[y * w + cx].c = ' ';
	}
	else {
		memmove(&screen[offset_to], &screen[offset_from], n_characters_to_move * sizeof(screen[0]));
	}
}

void terminal::insert_line(const int y)
{
	int offset_to    = (y + 1) * w;
	int offset_from  = y * w;

	int n_characters_to_move = w * h - offset_to;

	if (n_characters_to_move == 0) {
		for(int cx = 0; cx<w; cx++)
			screen[y * w + cx].c = ' ';
	}
	else {
		memmove(&screen[offset_to], &screen[offset_from], n_characters_to_move * sizeof(screen[0]));
	}
}

void terminal::insert_character(const int n)
{
	int n_left      = w - x - 1;
	int offset_from = y * w + x;
	int offset_to   = y * w + x + 1;

	for(int i=0; i<n; i++)
		memmove(&screen[offset_to], &screen[offset_from], n_left * sizeof(screen[0]));
}

void terminal::process_escape(const char cmd, const std::string & parameters)
{
	std::vector<std::string> pars = split(parameters, ";");

	int                      par1 = pars.size() >= 1 ? std::atoi(pars[0].c_str()) : 0;
	int                      par2 = pars.size() >= 2 ? std::atoi(pars[1].c_str()) : 0;

	if (cmd == 'A') {  // cursor up
		y -= par1 ? par1 : 1;

		if (y < 0)
			y = 0;
	}
	else if (cmd == 'B') {  // cursor down
		y += par1 ? par1 : 1;

		if (y >= h)
			y = h - 1;
	}
	else if (cmd == 'b') { // repeat
		int n = par1 ? par1 : 1;

		char data[] = { last_character };

		for(int i=0; i<n; i++)
			process_input(data, sizeof data);
	}
	else if (cmd == 'C') {  // cursor forward
		x += par1 ? par1 : 1;

		if (x >= w)
			x = w - 1;
	}
	else if (cmd == 'D') {  // cursor backward
		x -= par1 ? par1 : 1;

		if (x < 0)
			x = 0;
	}
	else if (cmd == 'd') {  // set y(?)
		y = par1 ? par1 - 1 : 0;

		if (y < 0)
			y = 0;
		else if (y >= h)
			y = h - 1;
	}
	else if (cmd == 'G') {  // cursor horizontal absolute
		x = par1 ? par1 - 1 : 0;

		if (x < 0)
			x = 0;
		else if (x >= w)
			x = w - 1;
	}
	else if (cmd == 'H') {  // set position
		y = par1 ? par1 - 1 : 0;

		if (y < 0)
			y = 0;
		else if (y >= h)
			y = h - 1;

		if (pars.size() >= 2) {
			x = par2 ? par2 - 1 : 0;

			if (x < 0)
				x = 0;
			else if (x >= w)
				x = w - 1;
		}
	}
	else if (cmd == 'J') {
		int val = 0;

		if (pars.size() >= 1)
			val = std::atoi(pars[0].c_str());

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

		for(int pos=start_pos; pos<end_pos; pos++) {
			screen[pos].c           = ' ';
			screen[pos].fg_col_ansi = fg_col_ansi;
			screen[pos].bg_col_ansi = bg_col_ansi;
			screen[pos].attr        = attr;
		}
	}
	else if (cmd == 'K') {
		int val = 0;

		if (pars.size() >= 1)
			val = std::atoi(pars[0].c_str());

		int start_x = 0;
		int end_x   = w;

		if (val == 0)
			start_x = x;
		else if (val == 1)
			end_x   = x + 1;
		else if (val == 2) {
			// use defaults
		}

		for(int cx=start_x; cx<end_x; cx++) {
			int offset = y * w + cx;

			screen[offset].c           = ' ';
			screen[offset].fg_col_ansi = fg_col_ansi;
			screen[offset].bg_col_ansi = bg_col_ansi;
			screen[offset].attr        = attr;
		}
	}
	else if (cmd == 'L') {
		for(int i=0; i<(par1 ? par1 : 1); i++)
			insert_line(y);

		x = 0;
	}
	else if (cmd == 'M') {
		for(int i=0; i<(par1 ? par1 : 1); i++)
			delete_line(y);

		x = 0;
	}
	else if (cmd == 'm') {
		if (pars.empty())
			fg_col_ansi = 7, bg_col_ansi = 0, attr = 0;
		else {
			for(auto & par : pars) {
				int par_val = std::atoi(par.c_str());

				if (par_val >= 30 && par_val <= 37)  // fg color
					fg_col_ansi = par_val - 30;
				else if (par_val == 39)
					fg_col_ansi = 7;
				else if (par_val == 49)
					bg_col_ansi = 0;
				else if (par_val >= 40 && par_val <= 47)  // bg color
					bg_col_ansi = par_val - 40;
				else if (par_val == 0)  // reset
					fg_col_ansi = bg_col_ansi = attr = 0;
				else if (par_val == 1)  // bold
					attr |= A_BOLD;
				else if (par_val == 2)  // faint
					attr = attr & ~A_BOLD;
				else if (par_val == 7)  // inverse video
					attr ^= A_INVERSE;
				else if (par_val >= 10 && par_val <= 19) {
					// font selection
				}
				else {
					dolog(ll_info, "code %d for 'm' not supported", par_val);
				}
			}
		}
	}
	else if (cmd == 'X') {  // erase character
		int offset = y * w + x;

		if (par1 == 0)
			par1 = 1;

		for(int i=0; i<par1; i++) {
			screen[offset + i].c           = ' ';
			screen[offset + i].fg_col_ansi = fg_col_ansi;
			screen[offset + i].bg_col_ansi = bg_col_ansi;
			screen[offset + i].attr        = attr;
		}
	}
	else if (cmd == '@') {  // insert character
		insert_character(par1 ? par1 : 1);
	}
	else {
		dolog(ll_info, "Escape ^[[ %s %c not supported", parameters.c_str(), cmd);
	}
}

void terminal::process_input(const char *const in, const size_t len)
{
	for(size_t i=0; i<len; i++) {
		if (in[i] == 13)  // carriage return
			x = 0;
		else if (in[i] == 10)  // new line
			y++;
		else if (in[i] == 8) {  // backspace
			if (x)
				x--;
			else if (y)
				x = 0, y--;
		}
		else if (in[i] == 9) {  // tab
			x &= ~7;
			x += 8;

			if (x >= w)
				x -= w, y++;
		}
		// ANSI escape handling
		else if (in[i] == 27 && escape_state == E_NONE)
			escape_state = E_ESC;
		else if (in[i] == '[' && escape_state == E_ESC)
			escape_state = E_BRACKET;
		else if (((in[i] >= '0' && in[i] <= '9') || in[i] == ';') && (escape_state == E_BRACKET || escape_state == E_VALUES)) {
			if (escape_state == E_BRACKET)
				escape_state = E_VALUES;

			escape_value += in[i];
		}
		else if ((in[i] >= 0x40 && in[i] <= 0x7e) && (escape_state == E_BRACKET || escape_state == E_VALUES)) {
			process_escape(in[i], escape_value);

			escape_state = E_NONE;
			escape_value.clear();
		}
		// "regular" text
		else {
			screen[y * w + x].c           = in[i];
			screen[y * w + x].fg_col_ansi = fg_col_ansi;
			screen[y * w + x].bg_col_ansi = bg_col_ansi;
			screen[y * w + x].attr        = attr;

			x++;

			if (x == w) {
				x = 0;
				y++;
			}

			last_character = in[i];
		}

		if (y == h) {
			delete_line(0);
			
			y = h - 1;
		}
	}

	std::unique_lock<std::mutex> lck(lock);

	latest_update = get_ms();

	cond.notify_all();
}

void terminal::process_input(const std::string & in)
{
	process_input(in.c_str(), in.size());
}

void terminal::render(uint64_t *const ts_after, uint8_t **const out, int *const out_w, int *const out_h)
{
	std::unique_lock<std::mutex> lck(lock);

	while(latest_update <= *ts_after && !*stop_flag)
		cond.wait_for(lck, std::chrono::milliseconds(500));

	*ts_after = latest_update;

	constexpr const int char_w = 8;
	constexpr const int char_h = 16;

	*out = reinterpret_cast<uint8_t *>(calloc(1, w * char_w * h * char_h * 3));

	int pixels_per_row = w * char_w;
	int bytes_per_row  = pixels_per_row * 3;

	for(int cy=0; cy<h; cy++) {
		for(int cx=0; cx<w; cx++) {
			int  offset      = cy * w + cx;
			char c           = screen[offset].c;

			int color_offset = screen[offset].attr & A_BOLD ? 1 : 0;

			int fg_color     = screen[offset].fg_col_ansi;
			int bg_color     = screen[offset].bg_col_ansi;

			if (fg_color == bg_color)
				fg_color = 7, bg_color = 0;

			rgb_t fg         = color_map[color_offset][fg_color];
			rgb_t bg         = color_map[color_offset][bg_color];

			if (screen[offset].attr & A_INVERSE)
				std::swap(fg, bg);

			if (c != 0) {
				const uint8_t *const char_bitmap = f->get_char_pointer(c);

				for(int py=0; py<16; py++) {
					uint8_t scanline = char_bitmap[py];

					for(int px=0; px<8; px++) {
						int  offset = cy * bytes_per_row * char_h + py * bytes_per_row + cx * char_w * 3 + px * 3;

						bool bit    = !!(scanline & (1 << (7 - px)));

						(*out)[offset + 0] = bit ? fg.r : bg.r;
						(*out)[offset + 1] = bit ? fg.g : bg.g;
						(*out)[offset + 2] = bit ? fg.b : bg.b;
					}
				}
			}
		}
	}

	*out_w = pixels_per_row;
	*out_h = h * char_h;
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
