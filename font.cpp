// (C) 2017-2024 by folkert van heusden, released under Apache License v2.0
#include <mutex>
#include <string>
#include <fontconfig/fontconfig.h>
#include <freetype/ftglyph.h>

#include "error.h"
#include "font.h"
#include "logging.h"


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
		int glyph_index = FT_Get_Char_Index(faces.at(0), c);

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

int font::get_intensity_multiplier(const intensity_t i)
{
	if (i == intensity_t::I_DIM)
		return 145;

	if (i == intensity_t::I_BOLD)
		return 255;

	return 200;
}

void font::draw_glyph_bitmap(const FT_Bitmap *const bitmap, const int height, const FT_Int x, const FT_Int y, const rgb_t & fg, const rgb_t & bg, const intensity_t intensity, const bool invert, const bool underline, const bool strikethrough, uint8_t *const dest, const int dest_width, const int dest_height)
{
	const int bytes = dest_width * dest_height * 3;

	uint8_t max = get_intensity_multiplier(intensity);

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

				int screen_buffer_offset  = screen_y * dest_width * 3 + x * 3 + glyph_x * 8;

				for(int xbit=0; xbit < 8; xbit++) {
					int pixel_v = b & 128 ? max : 0;

					b <<= 1;

					if (invert)
						pixel_v = max - pixel_v;

					int sub = max - pixel_v;

					// if (screen_buffer_offset < 0 || screen_buffer_offset >= bytes)
					//	printf("%d,%d | %d => %d\n", glyph_x, glyph_y, xbit, screen_buffer_offset);

					if (screen_buffer_offset >= 0) {
						dest[screen_buffer_offset + 0] = (pixel_v * fg.r + sub * bg.r) >> 8;
						dest[screen_buffer_offset + 1] = (pixel_v * fg.g + sub * bg.g) >> 8;
						dest[screen_buffer_offset + 2] = (pixel_v * fg.b + sub * bg.b) >> 8;
					}

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

			const int screen_buffer_offset = screen_y * dest_width * 3;

			for(unsigned glyph_x=0; glyph_x<bitmap->width; glyph_x++) {
				int screen_x = glyph_x + x;

				if (screen_x >= dest_width)
					break;

				int local_screen_buffer_offset = screen_buffer_offset + screen_x * 3;

				int io = glyph_y * bitmap->width + glyph_x;

				int pixel_v = bitmap->buffer[io] * max / 255;

				if (invert)
					pixel_v = max - pixel_v;

				int sub = max - pixel_v;

				dest[local_screen_buffer_offset + 0] = (pixel_v * fg.r + sub * bg.r) >> 8;
				dest[local_screen_buffer_offset + 1] = (pixel_v * fg.g + sub * bg.g) >> 8;
				dest[local_screen_buffer_offset + 2] = (pixel_v * fg.b + sub * bg.b) >> 8;
			}
		}
	}

	if (strikethrough) {
		const int middle_line = y + bitmap->rows / 2;

		int offset = middle_line * dest_width * 3 + x * 3;

		for(unsigned glyph_x=0; glyph_x<bitmap->width; glyph_x++) {
			int screen_buffer_offset = offset + glyph_x * 3;

			if (screen_buffer_offset >= 0) {
				dest[screen_buffer_offset + 0] = (max * fg.r) >> 8;
				dest[screen_buffer_offset + 1] = (max * fg.g) >> 8;
				dest[screen_buffer_offset + 2] = (max * fg.b) >> 8;
			}
		}
	}

	if (underline) {
		int pixel_v = invert ? 0 : max;

		for(unsigned int glyph_x=0; glyph_x<bitmap->width; glyph_x++) {
			int screen_x = glyph_x + x;

			if (screen_x >= dest_width)
				break;

			int screen_buffer_offset = (y + height - 1) * dest_width * 3 + screen_x * 3;

			if (screen_buffer_offset + 2 >= bytes)
				continue;

			dest[screen_buffer_offset + 0] = (pixel_v * fg.r) >> 8;
			dest[screen_buffer_offset + 1] = (pixel_v * fg.g) >> 8;
			dest[screen_buffer_offset + 2] = (pixel_v * fg.b) >> 8;
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

bool font::draw_glyph(const UChar32 utf_character, const int output_height, const intensity_t intensity, const bool invert, const bool underline, const bool strikethrough, const bool italic, const rgb_t & fg, const rgb_t & bg, const int x, const int y, uint8_t *const dest, const int dest_width, const int dest_height)
{
	std::vector<FT_Encoding> encodings { ft_encoding_symbol, ft_encoding_unicode };

	for(auto & encoding : encodings) {
		for(size_t face = 0; face<faces.size(); face++) {
			FT_Select_Charmap(faces.at(face), encoding);

			int glyph_index = FT_Get_Char_Index(faces.at(face), utf_character);

			if (glyph_index == 0 && face < faces.size() - 1)
				continue;

			if (FT_Load_Glyph(faces.at(face), glyph_index, 0))
				continue;

			// draw background
			uint8_t max = get_intensity_multiplier(intensity);
			uint8_t bg_r = invert ? fg.r * max / 255 : bg.r * max / 255;
			uint8_t bg_g = invert ? fg.g * max / 255 : bg.g * max / 255;
			uint8_t bg_b = invert ? fg.b * max / 255 : bg.b * max / 255;

			for(int cy=0; cy<output_height; cy++) {
				int offset_y = (y + cy) * dest_width * 3;

				for(int cx=0; cx<font_width; cx++) {
					int offset = offset_y + (x + cx) * 3;

					dest[offset + 0] = bg_r;
					dest[offset + 1] = bg_g;
					dest[offset + 2] = bg_b;
				}
			}

			FT_GlyphSlot slot = faces.at(face)->glyph;
			if (!slot)
				continue;
			FT_Glyph glyph { };
			FT_Get_Glyph(slot, &glyph);

			if (italic) {
				FT_Matrix matrix { };
				matrix.xx = 0x10000;
				matrix.xy = 0x5000;
				matrix.yx = 0;
				matrix.yy = 0x10000;
				if (FT_Glyph_Transform(glyph, &matrix, nullptr))
					dolog(ll_info, "transform error");
			}

			FT_Glyph_To_Bitmap(&glyph, FT_RENDER_MODE_NORMAL, nullptr, false);

			int          draw_x = x + font_width / 2 - slot->metrics.width / 128;
			int          draw_y = y + max_ascender / 64 - slot->bitmap_top;

			draw_glyph_bitmap(&reinterpret_cast<FT_BitmapGlyph>(glyph)->bitmap, output_height, draw_x, draw_y, fg, bg, intensity, invert, underline, strikethrough, dest, dest_width, dest_height);

			FT_Done_Glyph(glyph);

			return true;
		}
	}

	return false;
}
