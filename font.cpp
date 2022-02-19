#include "error.h"
#include "font.h"


font::font(const std::string & font_file)
{
	FILE *fh = fopen(font_file.c_str(), "r");

	if (fread(f, 16, 256, fh) != 256)
		error_exit(false, "font: \"%s\" is an invalid font file", font_file.c_str());

	fclose(fh);
}

font::~font()
{
}

const uint8_t *font::get_char_pointer(const char c) const
{
	return &f[uint8_t(c) * 16];
}
