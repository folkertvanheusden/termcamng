// (C) 2017-2022 by folkert van heusden, released under Apache License v2.0
#include <mutex>
#include <string>
#include <fontconfig/fontconfig.h>

#include "error.h"
#include "font.h"

FT_Library font::library;

std::mutex freetype2_lock;
std::mutex fontconfig_lock;

font::font(const std::vector<std::string> & font_files, const int font_height) : font_height(font_height)
{
	FT_Init_FreeType(&font::library);

	// freetype2 is not thread safe
	const std::lock_guard<std::mutex> lock(freetype2_lock);

	for(auto & font_file : font_files) {
		FT_Face face { 0 };

		int rc = FT_New_Face(library, font_file.c_str(), 0, &face);
		if (rc)
			error_exit(false, "cannot open font file %s: %x", font_file.c_str(), rc);

		FT_Select_Charmap(face, ft_encoding_unicode);

		FT_Set_Char_Size(face, font_height * 64, font_height * 64, 72, 72);

		faces.push_back(face);
	}

	// font '0' (first font) must contain all basic characters
	// determine dimensions of character set
	for(UChar32 c = 32; c < 127; c++) {
		int glyph_index   = FT_Get_Char_Index(faces.at(0), c);

		if (FT_Load_Glyph(faces.at(0), glyph_index, 0) == 0) {
			font_width   = std::max(font_width  , int(faces.at(0)->glyph->metrics.horiAdvance) / 64);

			max_ascender = std::max(max_ascender, int(faces.at(0)->glyph->metrics.horiBearingY));
		}
	}
}

font::~font()
{
	const std::lock_guard<std::mutex> lock(freetype2_lock);

	for(auto f : faces)
		FT_Done_Face(f);

	FT_Done_FreeType(font::library);
}

void font::draw_glyph_bitmap(const FT_Bitmap *const bitmap, const int height, const FT_Int x, const FT_Int y, const rgb_t & fg, const rgb_t & bg, const bool invert, const bool underline, uint8_t *const dest, const int dest_width, const int dest_height)
{
	const int bytes = dest_width * dest_height * 3;

	if (bitmap->pixel_mode == FT_PIXEL_MODE_MONO) {
		for(unsigned int glyph_y=0; glyph_y<bitmap->rows; glyph_y++) {
			int screen_y = glyph_y + y;

			if (screen_y < 0)
				continue;

			if (screen_y >= dest_height)
				break;

			// assuming width is always multiple of 8
			for(unsigned glyph_x=0; glyph_x<bitmap->width / 8; glyph_x++) {
				int io = glyph_y * bitmap->width / 8 + glyph_x;

				uint8_t b = bitmap->buffer[io];

				int screen_buffer_offset  = screen_y * dest_width * 3 + x * 3;

				for(int xbit=0; xbit < 8; xbit++) {
					int pixel_v = b & 128 ? 255 : 0;

					b <<= 1;

					if (invert)
						pixel_v = 255 - pixel_v;

					int sub = 255 - pixel_v;

					dest[screen_buffer_offset + 0] = (pixel_v * fg.r + sub * bg.r) >> 8;
					dest[screen_buffer_offset + 1] = (pixel_v * fg.g + sub * bg.g) >> 8;
					dest[screen_buffer_offset + 2] = (pixel_v * fg.b + sub * bg.b) >> 8;

					screen_buffer_offset += 3;
				}
			}
		}
	}
	else if (bitmap->pixel_mode == FT_PIXEL_MODE_GRAY) {
		for(unsigned int glyph_y=0; glyph_y<bitmap->rows; glyph_y++) {
			int screen_y = glyph_y + y;

			if (screen_y < 0)
				continue;

			if (screen_y >= dest_height)
				break;

			for(unsigned glyph_x=0; glyph_x<bitmap->width; glyph_x++) {
				int screen_x = glyph_x + x;

				if (screen_x >= dest_width)
					break;

				int screen_buffer_offset  = screen_y * dest_width * 3 + screen_x * 3;

				int io = glyph_y * bitmap->width + glyph_x;

				int pixel_v = bitmap->buffer[io];

				if (invert)
					pixel_v = 255 - pixel_v;

				int sub = 255 - pixel_v;

				dest[screen_buffer_offset + 0] = (pixel_v * fg.r + sub * bg.r) >> 8;
				dest[screen_buffer_offset + 1] = (pixel_v * fg.g + sub * bg.g) >> 8;
				dest[screen_buffer_offset + 2] = (pixel_v * fg.b + sub * bg.b) >> 8;
			}
		}

		if (underline) {
			int pixel_v = invert ? 0 : 255;

			for(int glyph_y=0; glyph_y<height; glyph_y++) {
				for(unsigned int glyph_x=0; glyph_x<bitmap->width; glyph_x++) {
					int screen_x = glyph_x + x;

					if (screen_x >= dest_width)
						break;

					int screen_buffer_offset = (y + height - (1 + glyph_y)) * dest_width * 3 + screen_x * 3;

					if (screen_buffer_offset + 2 >= bytes)
						continue;

					dest[screen_buffer_offset + 0] = (pixel_v * fg.r) >> 8;
					dest[screen_buffer_offset + 1] = (pixel_v * fg.g) >> 8;
					dest[screen_buffer_offset + 2] = (pixel_v * fg.b) >> 8;
				}
			}
		}
	}
}

int font::get_width() const
{
	return font_width;
}

int font::get_height() const
{
	return font_height;
}

bool font::draw_glyph(const UChar32 utf_character, const int output_height, const bool invert, const bool underline, const rgb_t & fg, const rgb_t & bg, const int x, const int y, uint8_t *const dest, const int dest_width, const int dest_height)
{
	std::vector<FT_Encoding> encodings { ft_encoding_symbol, ft_encoding_unicode };

	for(auto & encoding : encodings) {
		for(size_t face = 0; face<faces.size(); face++) {
			FT_Select_Charmap(faces.at(face), encoding);

			int glyph_index = FT_Get_Char_Index(faces.at(face), utf_character);

			if (glyph_index == 0 && face < faces.size() - 1)
				continue;

			if (FT_Load_Glyph(faces.at(face), glyph_index, FT_LOAD_RENDER))
				continue;

			// draw background
			for(int cy=0; cy<output_height; cy++) {
				for(int cx=0; cx<font_width; cx++) {
					int offset = (y + cy) * dest_width * 3 + (x + cx) * 3;

					dest[offset + 0] = bg.r;
					dest[offset + 1] = bg.g;
					dest[offset + 2] = bg.b;
				}
			}

			FT_GlyphSlot slot   = faces.at(face)->glyph;

			int          draw_x = x + font_width / 2 - slot->metrics.width / 128;

			int          draw_y = y + max_ascender / 64 - slot->bitmap_top;

			draw_glyph_bitmap(&slot->bitmap, output_height, draw_x, draw_y, fg, bg, invert, underline, dest, dest_width, dest_height);

			return true;
		}
	}

	return false;
}
