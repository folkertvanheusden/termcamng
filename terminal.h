#include <cstddef>
#include <stdint.h>
#include <string>

#include "font.h"


typedef struct {
	char c;
	int fg_col_ansi, bg_col_ansi;
	int attr;
} pos_t;

class terminal {
private:
	font      *const f { nullptr };
	const int        w { 80 };
	const int        h { 25 };
	pos_t           *screen { nullptr };
	int              x { 0 };
	int              y { 0 };

public:
	terminal(font *const f, const int w, const int h);
	virtual ~terminal();

	void delete_line(const int y);

	void process_input(const char *const in, const size_t len);
	void process_input(const std::string & in);

	void render(uint8_t **const out, int *const out_w, int *const out_h);
};
