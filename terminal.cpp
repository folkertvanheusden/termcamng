#include <cstring>
#include <stdlib.h>
#include <string>
#include <vector>

#include "str.h"
#include "terminal.h"


terminal::terminal(font *const f, const int w, const int h) :
	f(f),
	w(w), h(h)
{
	screen = new pos_t[w * h]();
}

terminal::~terminal()
{
	delete [] screen;
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

void terminal::process_escape(const char cmd, const std::string & parameters)
{
	std::vector<std::string> pars = split(parameters, ";");

	int                      par1 = pars.size() >= 1 ? std::atoi(pars[0].c_str()) : 1;
	int                      par2 = pars.size() >= 2 ? std::atoi(pars[1].c_str()) : 1;

	if (cmd == 'A') {  // cursor up
		if (y)
			y -= par1;
	}
	else if (cmd == 'B') {  // cursor down
		if (y < h - 1)
			y += par1;
	}
	else if (cmd == 'C') {  // cursor forward
		if (x < w - 1)
			x += par1;
	}
	else if (cmd == 'D') {  // cursor backward
		if (x)
			x -= par1;
	}
	else if (cmd == 'G') {  // cursor horizontal absolute
		x = par1 - 1;

		if (x < 0)
			x = 0;
		else if (x >= w)
			x = w - 1;
	}
	else if (cmd == 'H') {  // set position
		y = par1;

		if (y < 0)
			y = 0;
		else if (y >= h)
			y = h - 1;

		if (pars.size() >= 2) {
			x = par2;

			if (x < 0)
				x = 0;
			else if (x >= w)
				x = w - 1;
		}
	}
	else {
		printf("Escape ^[[ %s %c not supported\n", parameters.c_str(), cmd);
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
		else if (((in[i] >= 'a' && in[i] <= 'z') || (in[i] >= 'A' && in[i] <= 'Z')) && (escape_state == E_BRACKET || escape_state == E_VALUES)) {
			process_escape(in[i], escape_value);

			escape_state = E_NONE;
			escape_value.clear();
		}
		// "regular" text
		else {
			screen[y * w + x].c = in[i];

			x++;

			if (x == w) {
				x = 0;
				y++;
			}
		}

		if (y == h) {
			delete_line(0);
			
			y = h - 1;
		}
	}
}

void terminal::process_input(const std::string & in)
{
	process_input(in.c_str(), in.size());
}

void terminal::render(uint8_t **const out, int *const out_w, int *const out_h)
{
	constexpr const int char_w = 8;
	constexpr const int char_h = 16;

	*out = reinterpret_cast<uint8_t *>(calloc(1, w * char_w * h * char_h * 3));

	int pixels_per_row = w * char_w;
	int bytes_per_row  = pixels_per_row * 3;

	for(int cy=0; cy<h; cy++) {
		for(int cx=0; cx<w; cx++) {
			char c = screen[cy * w + cx].c;

			if (c > 0 && c < 128) {
				const uint8_t *const char_bitmap = f->get_char_pointer(c);

				for(int py=0; py<16; py++) {
					uint8_t scanline = char_bitmap[py];

					for(int px=0; px<8; px++) {
						int  offset = cy * bytes_per_row * char_h + py * bytes_per_row + cx * char_w * 3 + px * 3;

						bool bit    = !!(scanline & (1 << (7 - px)));

						(*out)[offset + 0] = bit * 255;
						(*out)[offset + 1] = bit * 255;
						(*out)[offset + 2] = bit * 255;
					}
				}
			}
		}
	}

	*out_w = pixels_per_row;
	*out_h = h * char_h;
}
