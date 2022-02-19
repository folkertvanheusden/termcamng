#include <cstring>
#include <stdlib.h>
#include <string>

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
	int offset_to    = y * w;
	int offset_from  = (y + 1) * w;

	int n_characters_to_move = w * h - offset_from;

	if (n_characters_to_move == 0) {
		for(int cx = 0; cx<w; cx++)
			screen[y * w + cx].c = ' ';
	}
	else {
		memmove(&screen[offset_to], &screen[offset_from], n_characters_to_move);
	}
}

void terminal::process_input(const char *const in, const size_t len)
{
	for(size_t i=0; i<len; i++) {
		if (in[i] == 13)
			x = 0;
		else if (in[i] == 10)
			y++;
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
