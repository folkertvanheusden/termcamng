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
	screen[y * w + x].c           = c;
	screen[y * w + x].fg_col_ansi = fg_col_ansi;
	screen[y * w + x].fg_rgb      = fg_rgb;
	screen[y * w + x].bg_col_ansi = bg_col_ansi;
	screen[y * w + x].bg_rgb      = bg_rgb;
	screen[y * w + x].attr        = attr;

	x++;

	if (x == w) {
		x = 0;
		y++;
	}

	last_character = c;
}

std::optional<std::string> terminal::process_escape(const char cmd, const std::string & parameters)
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
	else if (cmd == 'C') {  // cursor forward
		x += evaluate_n(par1);

		if (x >= w) {
			dolog(ll_info, "%c: x=%d", cmd, x);
			x = w - 1;
		}
	}
	else if (cmd == 'D') {  // cursor backward
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
	else if (cmd == 'H' || cmd == 'f') {  // set position
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
	else if (cmd == 'M') {
		int n = evaluate_n(par1);

		for(int i=0; i<n; i++)
			delete_line(y);

		x = 0;
	}
	else if (cmd == 'm') {
		if (pars.empty())
			fg_col_ansi = 7, bg_col_ansi = 0, attr = 0;
		else {
			int rgb_fg_index = -1;
			bool is_fg = true;
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
				else if (par_val == 2) {  // faint or rgb selection
					if (fg_col_ansi == -1 && is_fg == true)
						rgb_fg_index = i + 1, i += 3;
					else if (bg_col_ansi == -1 && is_fg == false)
						rgb_bg_index = i + 1, i += 3;
					else if (fg_col_ansi != -1 && bg_col_ansi != -1)
						attr = attr & ~A_BOLD;
					else
						dolog(ll_info, "rgb selection failed (%d,%d / %d)", fg_col_ansi, bg_col_ansi, is_fg);
				}
				else if (par_val == 7)  // inverse video
					attr ^= A_INVERSE;
				else if (par_val >= 10 && par_val <= 19) {
					// font selection
				}
				else {
					dolog(ll_info, "code %d for 'm' not supported", par_val);
				}
			}

			if (rgb_fg_index != -1 && pars.size() - rgb_fg_index >= 3) {
				fg_rgb.r = std::atoi(pars.at(rgb_fg_index + 0).c_str());
				fg_rgb.g = std::atoi(pars.at(rgb_fg_index + 1).c_str());
				fg_rgb.b = std::atoi(pars.at(rgb_fg_index + 2).c_str());
			}

			if (rgb_bg_index != -1 && pars.size() - rgb_bg_index >= 3) {
				bg_rgb.r = std::atoi(pars.at(rgb_bg_index + 0).c_str());
				bg_rgb.g = std::atoi(pars.at(rgb_bg_index + 1).c_str());
				bg_rgb.b = std::atoi(pars.at(rgb_bg_index + 2).c_str());
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
	else {
		dolog(ll_info, "Escape ^[[ %s %c not supported", parameters.c_str(), cmd);
	}

	return send_back;
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
		else if (in[i] == '[' && escape_state == E_ESC)
			escape_state = E_BRACKET, utf8_len = 0;
		else if (escape_state == E_BRACKET || escape_state == E_VALUES) {
			if ((in[i] >= '0' && in[i] <= '9') || in[i] == ';') {
				if (escape_state == E_BRACKET)
					escape_state = E_VALUES;

				escape_value += in[i];
			}
			else if (in[i] >= 0x40 && in[i] <= 0x7e) {
				// because of 'b'
				std::string temp = escape_value;

				escape_state = E_NONE;
				escape_value.clear();

				send_back = process_escape(in[i], temp);
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

void terminal::render(uint64_t *const ts_after, const int max_wait, uint8_t **const out, int *const out_w, int *const out_h) const
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

			int      bold         = screen[offset].attr & A_BOLD ? 1 : 0;

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
			bool     underline    = false;

			int     x            = cx * char_w;
			int     y            = cy * char_h;

			if (!f->draw_glyph(c, char_h, inverse, underline, fg, bg, x, y, *out, *out_w, *out_h)) {
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
