#include <cstddef>
#include <stdint.h>
#include <string>

#include "font.h"


#define A_BOLD    (1 << 0)
#define A_INVERSE (1 << 1)

typedef enum { E_NONE, E_ESC, E_BRACKET, E_VALUES } escape_state_t;

typedef struct {
	char c;
	int fg_col_ansi, bg_col_ansi;
	int attr;
} pos_t;

typedef struct {
	int r;
	int g;
	int b;
} rgb_t;

class terminal {
private:
	font      *const f { nullptr };
	const int        w { 80 };
	const int        h { 25 };
	pos_t           *screen { nullptr };
	int              x { 0 };
	int              y { 0 };
	escape_state_t   escape_state { E_NONE };
	std::string      escape_value;
	int              fg_col_ansi { 37 };
	int              bg_col_ansi { 40 };
	int              attr        {  0 };
	rgb_t            color_map[2][8];
	char             last_character { ' ' };

public:
	terminal(font *const f, const int w, const int h);
	virtual ~terminal();

	int  get_width()  const { return w; };
	int  get_height() const { return h; };

	char get_char_at(const int x, const int y) const;

	void delete_line(const int y);
	void insert_line(const int y);

	void insert_character(const int n);

	void process_escape(const char cmd, const std::string & parameters);

	void process_input(const char *const in, const size_t len);
	void process_input(const std::string & in);

	void render(uint8_t **const out, int *const out_w, int *const out_h);
};
