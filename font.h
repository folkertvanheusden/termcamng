#include <stdint.h>
#include <string>


class font
{
private:
	// font must be 8 x 16 pixels, 1 byte per scan-line
	uint8_t f[256 * 16];

public:
	font(const std::string & font_file);
	~font();

	const uint8_t *get_char_pointer(const char c) const;
};
