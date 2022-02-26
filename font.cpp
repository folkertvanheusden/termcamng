// (C) 2017-2022 by folkert van heusden, released under Apache License v2.0
#include <mutex>
#include <string>
#include <fontconfig/fontconfig.h>

#include "error.h"
#include "font.h"

std::map<std::string, FT_Face> font::font_cache;

FT_Library                     font::library;

std::mutex                     freetype2_lock;
std::mutex                     fontconfig_lock;

font::font(const std::string & font_file, const int font_height) : font_height(font_height)
{
	FT_Init_FreeType(&font::library);

	// freetype2 is not thread safe
	const std::lock_guard<std::mutex> lock(freetype2_lock);

	face = load_font(font_file);

	FT_Select_Charmap(face, ft_encoding_unicode);

	FT_Set_Char_Size(face, font_height * 64, font_height * 64, 72, 72);

	// determine dimensions of character set
	for(UChar32 c = 32; c < 127; c++) {
		int glyph_index   = FT_Get_Char_Index(face, c);

		if (FT_Load_Glyph(face, glyph_index, FT_LOAD_RENDER) == 0) {
			font_width   = std::max(font_width  , int(face->glyph->metrics.horiAdvance) / 64);

			max_ascender = std::max(max_ascender, int(face->glyph->metrics.horiBearingY));
		}
	}
}

font::~font()
{
	const std::lock_guard<std::mutex> lock(freetype2_lock);

	std::map<std::string, FT_Face>::iterator it = font_cache.begin();

	while(it != font_cache.end()) {
		FT_Done_Face(it -> second);
		it++;
	}

	FT_Done_FreeType(font::library);
}

FT_Face font::load_font(const std::string & font_filename)
{
	FT_Face face = nullptr;

	auto it = font_cache.find(font_filename);

	if (it == font_cache.end()) {
		int rc = FT_New_Face(library, font_filename.c_str(), 0, &face);
		if (rc)
			error_exit(false, "cannot open font file %s: %x", font_filename.c_str(), rc);

		font_cache.insert(std::pair<std::string, FT_Face>(font_filename, face));
	}
	else {
		face = it -> second;
	}

	return face;
}

void font::draw_glyph_bitmap(const FT_Bitmap *const bitmap, const int height, const FT_Int x, const FT_Int y, const rgb_t & fg, const rgb_t & bg, const bool invert, const bool underline, uint8_t *const dest, const int dest_width, const int dest_height)
{
	const int bytes  = dest_width * dest_height * 3;

	int       startx = x < 0 ? -x : 0;

	int       endx   = bitmap->width;

	for(unsigned int yo=0; yo<bitmap->rows; yo++) {
		int yu = yo + y;

		if (yu < 0)
			continue;

		if (yu >= dest_height)
			break;

		for(int xo=startx; xo<endx; xo++) {
			int xu = xo + x;

			int o = yu * dest_width * 3 + xu * 3;

			int pixel_v = bitmap->buffer[yo * bitmap->width + xo];

			if (invert)
				pixel_v = 255 - pixel_v;

			int sub = 255 - pixel_v;

			dest[o + 0] = (pixel_v * fg.r + sub * bg.r) >> 8;
			dest[o + 1] = (pixel_v * fg.g + sub * bg.g) >> 8;
			dest[o + 2] = (pixel_v * fg.b + sub * bg.b) >> 8;
		}
	}

	if (underline) {
		int pixel_v = invert ? 0 : 255;

		for(int yo=0; yo<height; yo++) {
			for(unsigned int xo=0; xo<bitmap->width; xo++) {
				int xu = xo + x;

				if (xu >= endx)
					break;

				int o = (y + height - (1 + yo)) * dest_width * 3 + xu * 3;

				if (o + 2 >= bytes)
					continue;

				dest[o + 0] = (pixel_v * fg.r) >> 8;
				dest[o + 1] = (pixel_v * fg.g) >> 8;
				dest[o + 2] = (pixel_v * fg.b) >> 8;
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
	int glyph_index   = FT_Get_Char_Index(face, utf_character);

	if (FT_Load_Glyph(face, glyph_index, FT_LOAD_RENDER))
		return false;

	// draw background
	for(int cy=0; cy<output_height; cy++) {
		for(int cx=0; cx<font_width; cx++) {
			int offset = (y + cy) * dest_width * 3 + (x + cx) * 3;

			dest[offset + 0] = bg.r;
			dest[offset + 1] = bg.g;
			dest[offset + 2] = bg.b;
		}
	}

	FT_GlyphSlot slot = face->glyph;

	int draw_y = y + max_ascender / 64 - slot->bitmap_top;

	draw_glyph_bitmap(&slot->bitmap, output_height, x, draw_y, fg, bg, invert, underline, dest, dest_width, dest_height);

	return true;
}

// from http://stackoverflow.com/questions/10542832/how-to-use-fontconfig-to-get-font-list-c-c
std::string find_font_by_name(const std::string & font_name, const std::string & default_font_file)
{
	std::string font_file = default_font_file;

	const std::lock_guard<std::mutex> lock(fontconfig_lock);

	FcConfig* config = FcInitLoadConfigAndFonts();

	// configure the search pattern, 
	// assume "name" is a std::string with the desired font name in it
	FcPattern* pat = FcNameParse((const FcChar8*)(font_name.c_str()));

	if (pat) {
		if (FcConfigSubstitute(config, pat, FcMatchPattern)) {
			FcDefaultSubstitute(pat);

			// find the font
			FcResult   result = FcResultNoMatch;
			FcPattern *font   = FcFontMatch(config, pat, &result);

			if (font) {
				FcChar8* file = nullptr;

				if (FcPatternGetString(font, FC_FILE, 0, &file) == FcResultMatch && file != nullptr) {
					// save the file to another std::string
					font_file = reinterpret_cast<const char *>(file);
				}

				FcPatternDestroy(font);
			}
		}

		FcPatternDestroy(pat);
	}

	return font_file;
}
