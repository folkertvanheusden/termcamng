// (C) 2017-2024 by folkert van heusden, released under Apache License v2.0
#include <cassert>
#include <mutex>
#include <string>
#include <fontconfig/fontconfig.h>
#include <freetype/ftbitmap.h>
#include <freetype/ftglyph.h>

#include "error.h"
#include "font.h"
#include "logging.h"


FT_Library font::library;

std::mutex freetype2_lock;
std::mutex fontconfig_lock;

font::font(const std::vector<std::string> & font_files, std::optional<int> font_width_in, const int font_height) : font_height(font_height)
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

		if (FT_Set_Char_Size(face, font_width_in.has_value() ? font_width_in.value() * 64 : 0, font_height * 64, 72, 72))
			FT_Select_Size(face, 0);

		faces.push_back(face);
	}

	glyph_cache.resize(faces.size());
	glyph_cache_italic.resize(faces.size());

	// font '0' (first font) must contain all basic characters
	// determine dimensions of character set
	int temp_width  = 0;
	int glyph_index = FT_Get_Char_Index(faces.at(0), 'm');
	for(UChar32 c = 33; c < 127; c++) {
		int glyph_index = FT_Get_Char_Index(faces.at(0), c);

		if (FT_Load_Glyph(faces.at(0), glyph_index, FT_LOAD_NO_BITMAP | FT_LOAD_COLOR) == 0) {
			temp_width   = std::max(temp_width  , int(faces.at(0)->glyph->metrics.horiAdvance) / 64);  // width should be all the same!
			max_ascender = std::max(max_ascender, int(faces.at(0)->glyph->metrics.horiBearingY));
		}
	}

	if (this->font_height == 0)
		this->font_height = max_ascender / 64;

	if (font_width_in.has_value() == false)
		font_width = temp_width;
	else
		font_width = font_width_in.value();
}

font::~font()
{
	const std::lock_guard<std::mutex> lock(freetype2_lock);

	for(auto & face: glyph_cache) {
		for(auto & element: face)
			FT_Bitmap_Done(library, &element.second.bitmap);
	}

	for(auto & face: glyph_cache_italic) {
		for(auto & element: face)
			FT_Bitmap_Done(library, &element.second.bitmap);
	}

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

void font::draw_glyph_bitmap_low(const FT_Bitmap *const bitmap, const rgb_t & fg, const rgb_t & bg, const bool has_color, const intensity_t intensity, const bool invert, const bool underline, const bool strikethrough, uint8_t **const result, int *const result_width, int *const result_height)
{
	const uint8_t max = get_intensity_multiplier(intensity);

	*result_height = bitmap->rows;

	if (bitmap->pixel_mode == FT_PIXEL_MODE_MONO) {
		*result_width = bitmap->width;
		*result       = new uint8_t[*result_height * *result_width * 3]();

		for(unsigned int glyph_y=0; glyph_y<bitmap->rows; glyph_y++) {
			int screen_y = glyph_y;

			// assuming width is always multiple of 8
			for(unsigned glyph_x=0; glyph_x<bitmap->width / 8; glyph_x++) {
				int     io = glyph_y * bitmap->width / 8 + glyph_x;
				uint8_t b  = bitmap->buffer[io];

				int screen_buffer_offset = screen_y * *result_width * 3 + glyph_x * 3 + glyph_x * 8;

				for(int xbit=0; xbit < 8; xbit++) {
					int pixel_v = b & 128 ? max : 0;

					b <<= 1;

					if (invert)
						pixel_v = max - pixel_v;

					int sub = max - pixel_v;

					if (screen_buffer_offset >= 0) {
						(*result)[screen_buffer_offset + 0] = (pixel_v * fg.r + sub * bg.r) >> 8;
						(*result)[screen_buffer_offset + 1] = (pixel_v * fg.g + sub * bg.g) >> 8;
						(*result)[screen_buffer_offset + 2] = (pixel_v * fg.b + sub * bg.b) >> 8;
					}

					screen_buffer_offset += 3;
				}
			}
		}
	}
	else if (bitmap->pixel_mode == FT_PIXEL_MODE_GRAY) {
		*result_width = bitmap->width;
		*result       = new uint8_t[*result_height * *result_width * 3]();

		for(int glyph_y=0; glyph_y<bitmap->rows; glyph_y++) {
			int screen_y = glyph_y;
			int screen_buffer_offset = screen_y * *result_width * 3;
			int io_base = glyph_y * bitmap->width;

			for(int glyph_x=0; glyph_x<bitmap->width; glyph_x++) {
				int screen_x = glyph_x;
				int local_screen_buffer_offset = screen_buffer_offset + screen_x * 3;
				int io = io_base + glyph_x;

				int pixel_v = bitmap->buffer[io] * max / 255;

				if (invert)
					pixel_v = max - pixel_v;

				int sub = max - pixel_v;

				(*result)[local_screen_buffer_offset + 0] = (pixel_v * fg.r + sub * bg.r) >> 8;
				(*result)[local_screen_buffer_offset + 1] = (pixel_v * fg.g + sub * bg.g) >> 8;
				(*result)[local_screen_buffer_offset + 2] = (pixel_v * fg.b + sub * bg.b) >> 8;
			}
		}
	}
	else if (bitmap->pixel_mode == FT_PIXEL_MODE_LCD) {
		*result_width = bitmap->width / 3;
		*result       = new uint8_t[*result_width * *result_height * 3]();

		for(int glyph_y=0; glyph_y<bitmap->rows; glyph_y++) {
			int screen_y = glyph_y;
			int screen_buffer_offset = screen_y * *result_width * 3;
			int io_base = glyph_y * bitmap->pitch;

			for(int glyph_x=0; glyph_x<*result_width; glyph_x++) {
				int screen_x = glyph_x;
				int local_screen_buffer_offset = screen_buffer_offset + screen_x * 3;

				int io = io_base + glyph_x * 3;

				int pixel_vr = bitmap->buffer[io + 0] * max / 255;
				int pixel_vg = bitmap->buffer[io + 1] * max / 255;
				int pixel_vb = bitmap->buffer[io + 2] * max / 255;

				if (invert) {
					pixel_vr = max - pixel_vr;
					pixel_vg = max - pixel_vg;
					pixel_vb = max - pixel_vb;
				}

				if (has_color) {
					(*result)[local_screen_buffer_offset + 0] = pixel_vr;
					(*result)[local_screen_buffer_offset + 1] = pixel_vg;
					(*result)[local_screen_buffer_offset + 2] = pixel_vb;
				}
				else {
					int sub = max - pixel_vr;
					(*result)[local_screen_buffer_offset + 0] = (pixel_vr * fg.r + sub * bg.r) >> 8;
					(*result)[local_screen_buffer_offset + 1] = (pixel_vr * fg.g + sub * bg.g) >> 8;
					(*result)[local_screen_buffer_offset + 2] = (pixel_vr * fg.b + sub * bg.b) >> 8;
				}
			}
		}
	}
	else if (bitmap->pixel_mode == FT_PIXEL_MODE_BGRA) {
		*result_width = bitmap->width;
		*result       = new uint8_t[*result_width * *result_height * 3]();

		for(int glyph_y=0; glyph_y<bitmap->rows; glyph_y++) {
			int screen_y = glyph_y;
			int screen_buffer_offset = screen_y * *result_width * 3;
			int io_base = glyph_y * bitmap->pitch;

			for(int glyph_x=0; glyph_x<*result_width; glyph_x++) {
				int screen_x = glyph_x;
				int local_screen_buffer_offset = screen_buffer_offset + screen_x * 3;

				int io = io_base + glyph_x * 4;

				int pixel_vr = bitmap->buffer[io + 2] * max / 255;
				int pixel_vg = bitmap->buffer[io + 1] * max / 255;
				int pixel_vb = bitmap->buffer[io + 0] * max / 255;

				if (invert) {
					pixel_vr = max - pixel_vr;
					pixel_vg = max - pixel_vg;
					pixel_vb = max - pixel_vb;
				}

				if (has_color) {
					(*result)[local_screen_buffer_offset + 0] = pixel_vr;
					(*result)[local_screen_buffer_offset + 1] = pixel_vg;
					(*result)[local_screen_buffer_offset + 2] = pixel_vb;
				}
				else {
					int sub = max - pixel_vr;
					(*result)[local_screen_buffer_offset + 0] = (pixel_vr * fg.r + sub * bg.r) >> 8;
					(*result)[local_screen_buffer_offset + 1] = (pixel_vr * fg.g + sub * bg.g) >> 8;
					(*result)[local_screen_buffer_offset + 2] = (pixel_vr * fg.b + sub * bg.b) >> 8;
				}
			}
		}
	}
	else {
		if (render_mode_error == false) {
			render_mode_error = true;

			dolog(ll_error, "PIXEL MODE %d NOT IMPLEMENTED", bitmap->pixel_mode);
		}
	}

	if (strikethrough) {
		const int middle_line = *result_height / 2;
		const int offset      = middle_line * *result_width * 3;

		for(unsigned glyph_x=0; glyph_x<*result_width; glyph_x++) {
			int screen_buffer_offset = offset + glyph_x * 3;

			if (screen_buffer_offset >= 0) {
				(*result)[screen_buffer_offset + 0] = (max * fg.r) >> 8;
				(*result)[screen_buffer_offset + 1] = (max * fg.g) >> 8;
				(*result)[screen_buffer_offset + 2] = (max * fg.b) >> 8;
			}
		}
	}

	if (underline) {
		int pixel_v = invert ? 0 : max;

		for(unsigned int glyph_x=0; glyph_x<*result_width; glyph_x++) {
			int screen_x = glyph_x;

			if (screen_x >= *result_width)
				break;

			int screen_buffer_offset = (*result_height - 2) * *result_width * 3 + screen_x * 3;

			(*result)[screen_buffer_offset + 0] = (pixel_v * fg.r) >> 8;
			(*result)[screen_buffer_offset + 1] = (pixel_v * fg.g) >> 8;
			(*result)[screen_buffer_offset + 2] = (pixel_v * fg.b) >> 8;
		}
	}
}

typedef struct {
        int n;
        double r, g, b;
} pixel_t;

void font::draw_glyph_bitmap(const glyph_cache_entry_t *const glyph, const FT_Int dest_x, const FT_Int dest_y, const rgb_t & fg, const rgb_t & bg, const bool has_color, const intensity_t intensity, const bool invert, const bool underline, const bool strikethrough, uint8_t *const dest, const int dest_width, const int dest_height)
{
	uint8_t *result        = nullptr;
	int      result_width  = 0;
	int      result_height = 0;
	draw_glyph_bitmap_low(&glyph->bitmap, fg, bg, has_color, intensity, invert, underline, strikethrough, &result, &result_width, &result_height);

	// resize & copy to x, y
	if (result_width + glyph->horiBearingX / 64 > font_width || result_height > font_height) {
		const double x_scale_temp      =        font_width   / (result_width + glyph->horiBearingX / 64.);
		const double y_scale_temp      = double(font_height) / result_height;
		const double smallest_scale    = std::min(x_scale_temp, y_scale_temp);
		const double scaled_bearing    = glyph->horiBearingX / 64 * smallest_scale;
		const double scaled_bitmap_top = glyph->bitmap_top        * smallest_scale;

		pixel_t *work = new pixel_t[font_width * font_height]();

		for(int y=0; y<result_height; y++) {
			int target_y     = y * smallest_scale;
			int put_offset_y = target_y * font_width;
			int get_offset_y = y * result_width * 3;

			for(int x=0; x<result_width; x++) {
				int target_x   = x * smallest_scale;
				int put_offset = put_offset_y + target_x;
				int get_offset = get_offset_y + x * 3;

				work[put_offset].n++;
				work[put_offset].r += result[get_offset + 0];
				work[put_offset].g += result[get_offset + 1];
				work[put_offset].b += result[get_offset + 2];
			}
		}

		// TODO: check for out of bounds writes
		int work_dest_y = dest_y + max_ascender / 64.0 - scaled_bitmap_top;
		int use_height  = std::min(dest_height - work_dest_y, font_height);

		for(int y=0; y<use_height; y++) {
			int yo  = y * font_width;
			int temp = y + work_dest_y;
			if (temp < 0)
				continue;
			int o   = temp * dest_width * 3 + dest_x * 3 + scaled_bearing;

			for(int x=0, i = yo; x<font_width; x++, i++, o += 3) {
				if (work[i].n) {
					dest[o + 0] = work[i].r / work[i].n;
					dest[o + 1] = work[i].g / work[i].n;
					dest[o + 2] = work[i].b / work[i].n;
				}
				else {
					dest[o + 0] =
					dest[o + 1] =
					dest[o + 2] = 0;
				}
			}
		}

		delete [] work;
	}
	else {

		int work_dest_x = dest_x + glyph->horiBearingX / 64;
		int use_width   = std::min(dest_width  - work_dest_x, result_width);
		int work_dest_y = dest_y + max_ascender / 64.0 - glyph->bitmap_top;
		int use_height  = std::min(dest_height - work_dest_y, result_height);

		for(int y=0; y<use_height; y++) {
			int temp = work_dest_y + y;

			if (temp >= 0)
				memcpy(&dest[temp * dest_width * 3 + work_dest_x * 3], &result[result_width * y * 3], use_width * 3);
		}
	}

	delete [] result;
}

int font::get_width() const
{
	return font_width;
}

int font::get_height() const
{
	return font_height;
}

bool font::draw_glyph(const UChar32 utf_character, const intensity_t intensity, const bool invert, const bool underline, const bool strikethrough, const bool italic, const rgb_t & fg, const rgb_t & bg, const int x, const int y, uint8_t *const dest, const int dest_width, const int dest_height)
{
	std::vector<FT_Encoding> encodings { ft_encoding_symbol, ft_encoding_unicode };

	for(int color = 0; color<2; color++) {
		for(int bitmap = 0; bitmap<2; bitmap++) {
			for(auto & encoding : encodings) {
				for(size_t face = 0; face<faces.size(); face++) {
					FT_Select_Charmap(faces.at(face), encoding);

					int glyph_index = FT_Get_Char_Index(faces.at(face), utf_character);
					if (glyph_index == 0 && face < faces.size() - 1)
						continue;

					auto it = italic ? glyph_cache_italic.at(face).find(glyph_index) : glyph_cache.at(face).find(glyph_index);

					if (it == glyph_cache.at(face).end()) {
						int color_choice  = face == 0 ? (color  == 0 ? 0 : FT_LOAD_COLOR | FT_LOAD_TARGET_LCD)     : (color == 0  ? FT_LOAD_COLOR | FT_LOAD_TARGET_LCD    : 0);
						int bitmap_choice = face == 0 ? (bitmap == 0 ? FT_LOAD_NO_BITMAP : 0) : (bitmap == 0 ? 0 : FT_LOAD_NO_BITMAP);
						if (FT_Load_Glyph(faces.at(face), glyph_index, bitmap_choice | color_choice))
							continue;

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

						if (glyph->format != FT_GLYPH_FORMAT_BITMAP) {
							if (FT_Glyph_To_Bitmap(&glyph, color_choice ? FT_RENDER_MODE_LCD : FT_RENDER_MODE_NORMAL, nullptr, true)) {
								FT_Done_Glyph(glyph);
								continue;
							}
						}

						glyph_cache_entry_t entry { };
						FT_Bitmap_Init(&entry.bitmap);
						FT_Bitmap_Copy(library, &reinterpret_cast<FT_BitmapGlyph>(glyph)->bitmap, &entry.bitmap);
						entry.horiBearingX = slot->metrics.horiBearingX;
						entry.bitmap_top   = slot->bitmap_top;

						FT_Done_Glyph(glyph);

						if (italic) {
							glyph_cache_italic.at(face).insert({ glyph_index, entry });
							it = glyph_cache_italic.at(face).find(glyph_index);
						}
						else {
							glyph_cache.at(face).insert({ glyph_index, entry });
							it = glyph_cache.at(face).find(glyph_index);
						}
					}

					// draw background
					uint8_t max = get_intensity_multiplier(intensity);
					uint8_t bg_r = invert ? fg.r * max / 255 : bg.r * max / 255;
					uint8_t bg_g = invert ? fg.g * max / 255 : bg.g * max / 255;
					uint8_t bg_b = invert ? fg.b * max / 255 : bg.b * max / 255;

					for(int cy=0; cy<font_height; cy++) {
						int offset_y = (y + cy) * dest_width * 3;

						for(int cx=0; cx<font_width; cx++) {
							int offset = offset_y + (x + cx) * 3;

							dest[offset + 0] = bg_r;
							dest[offset + 1] = bg_g;
							dest[offset + 2] = bg_b;
						}
					}

					draw_glyph_bitmap(&it->second, x, y, fg, bg, FT_HAS_COLOR(faces.at(face)), intensity, invert, underline, strikethrough, dest, dest_width, dest_height);

					return true;
				}
			}
		}
	}

	return false;
}
