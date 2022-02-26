// (C) 2017-2022 by folkert van heusden, released under Apache License v2.0
#pragma once

#include <map>
#include <mutex>
#include <optional>
#include <stdint.h>
#include <string>

#include <freetype2/ft2build.h>
#include FT_FREETYPE_H

#include <unicode/ustring.h>

#include "common.h"


#define DEFAULT_FONT_FILE "/usr/share/fonts/truetype/unifont/unifont.ttf"

extern std::mutex freetype2_lock;
extern std::mutex fontconfig_lock;

class font
{
protected:
	static std::map<std::string, FT_Face> font_cache;
	static FT_Library                     library;

	const int                             font_height;
	FT_Face                               face;

	FT_Face load_font(const std::string & font_filename);

	void draw_glyph_bitmap(const FT_Bitmap *const bitmap, const int output_height, const FT_Int x, const FT_Int y, const rgb_t & fg, const rgb_t & bg, const bool invert, const bool underline, uint8_t *const dest, const int dest_width, const int dest_height);

public:
	font(const std::string & font_file, const int font_height);
	virtual ~font();

	int  get_height() const;

	bool draw_glyph(const UChar32 utf_character, const int height, const bool invert, const bool underline, const rgb_t & fg, const rgb_t & bg, const int x, const int y, uint8_t *const dest, const int dest_width, const int dest_height);
};

std::string find_font_by_name(const std::string & font_name, const std::string & default_font_file);
