// (C) 2017-2024 by folkert van heusden, released under Apache License v2.0
#pragma once

#include <map>
#include <mutex>
#include <optional>
#include <stdint.h>
#include <string>
#include <vector>

#include <freetype2/ft2build.h>
#include FT_FREETYPE_H
#include <freetype/ftglyph.h>

#include <unicode/ustring.h>

#include "common.h"


#define DEFAULT_FONT_FILE "/usr/share/fonts/truetype/unifont/unifont.ttf"

extern std::mutex freetype2_lock;
extern std::mutex fontconfig_lock;

typedef struct {
	FT_Bitmap bitmap;
	int       horiBearingX;
	int       bitmap_top;
} glyph_cache_entry_t;

class font
{
public:
	enum intensity_t { I_NORMAL, I_BOLD, I_DIM };

protected:
	static FT_Library    library;

	int                  font_height  { 0 };
	int                  font_width   { 0 };
	int                  max_ascender { 0 };
	std::vector<FT_Face> faces;
	std::vector<std::map<int, glyph_cache_entry_t> > glyph_cache;
	std::vector<std::map<int, glyph_cache_entry_t> > glyph_cache_italic;
	bool                 render_mode_error { false };

	int get_intensity_multiplier(const intensity_t i);

	std::optional<std::tuple<int, int, int, int> > find_text_dimensions(const UChar32 c);

	void draw_glyph_bitmap_low(const FT_Bitmap *const bitmap, const int height, const rgb_t & fg, const rgb_t & bg, const intensity_t intensity, const bool invert, const bool underline, const bool strikethrough, uint8_t **const result, int *const result_width, int *const result_height);
	void draw_glyph_bitmap(const FT_Bitmap *const bitmap, const int output_height, const FT_Int x, const FT_Int y, const rgb_t & fg, const rgb_t & bg, const intensity_t i, const bool invert, const bool underline, const bool strikethrough, uint8_t *const dest, const int dest_width, const int dest_height);

public:
	font(const std::vector<std::string> & font_files, std::optional<int> font_width, const int font_height);
	virtual ~font();

	int  get_width() const;
	int  get_height() const;

	bool draw_glyph(const UChar32 utf_character, const int height, const intensity_t i, const bool invert, const bool underline, const bool strikethrough, const bool italic, const rgb_t & fg, const rgb_t & bg, const int x, const int y, uint8_t *const dest, const int dest_width, const int dest_height);
};
