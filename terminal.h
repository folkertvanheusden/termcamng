#pragma once
#include <atomic>
#include <condition_variable>
#include <cstddef>
#include <mutex>
#include <optional>
#include <stdint.h>
#include <string>

#include "common.h"
#include "font.h"


#define A_BOLD          (1 << 0)
#define A_DIM           (1 << 1)
#define A_INVERSE       (1 << 2)
#define A_UNDERLINE     (1 << 3)
#define A_STRIKETHROUGH (1 << 4)
#define A_BLINK         (1 << 5)
#define A_ITALIC        (1 << 6)

typedef enum { ET_NONE, ET_DCS, ET_CSI, ET_ST, ET_OSC } escape_type_t;

#define DLD(...)  do {                                    \
		std::string prefix = myformat("x: %03d, y: %02d, wrap: %d | ", x, y, wraparound); \
		std::string temp = myformat(__VA_ARGS__); \
                                                          \
		dolog(ll_debug, (prefix + temp).c_str()); \
	}                                                 \
	while(0);

typedef struct {
	uint32_t c;
	int      fg_col_ansi;
	rgb_t    fg_rgb;
	int      bg_col_ansi;
	rgb_t    bg_rgb;
	int      attr;
} pos_t;

class terminal {
private:
	font       *const f { nullptr };
	int               w { 80 };
	int               h { 25 };
	pos_t            *screen { nullptr };
	int               x { 0 };
	int               y { 0 };
	escape_type_t     escape_type { ET_NONE };
	std::string       escape_value;
	bool              escape      { false };
	int               fg_col_ansi { 37 };
	rgb_t             fg_rgb      { 255, 255, 255 };
	int               bg_col_ansi { 40 };
	rgb_t             bg_rgb      { 0, 0, 0 };
	int               attr        {  0 };
	rgb_t             color_map[2][8];
	rgb_t             color_map_256c[256];
	uint32_t          last_character { ' ' };
	uint64_t          latest_update { 0 };
	int               utf8_len    { 0 };
	uint32_t          utf8_code   { 0 };
	bool              OSC         { false };
	bool              blink_state { false };
	uint64_t          blink_switch_ts { 0 };
	bool              wraparound  { true  };
	std::vector<bool> h_tab_stops;
	std::vector<bool> v_tab_stops;
	bool              global_invert    { false };  // DECSNM
	bool		  smooth_scrolling { false };  // DECSCLM
	uint8_t          *frame_cache { nullptr };
	std::mutex        frame_cache_lock;
	bool              do_render   { false };

	mutable std::mutex              lock;
	mutable std::condition_variable cond;
	std::atomic_bool         *const stop_flag;

public:
	terminal(font *const f, const int w, const int h, std::atomic_bool *const stop_flag);
	virtual ~terminal();

	int  get_width()  const { return w; };
	int  get_height() const { return h; };

	std::pair<int, int> get_current_xy();

	char  get_char_at(const int x, const int y) const;
	pos_t get_cell_at(const int x, const int y) const;

	void delete_line(const int y);
	void insert_line(const int y);

	void insert_character(const int n);
	void delete_character(const int n);

	void erase_cell(const int x, const int y);
	void erase_line(const int cy);

	void emit_character(const uint32_t c);

	void reset_h_tab_stops();
	void reset_v_tab_stops();

	void resize_width(const int new_w);

	void do_next_line(const bool move_to_left, const bool do_scroll, const int n_lines);
	void do_prev_line(const bool move_to_left, const bool do_scroll, const int n_lines);

	std::optional<std::string> process_escape_CSI(const char cmd, const std::string & parameters);

	std::optional<std::string> process_input(const char *const in, const size_t len);
	std::optional<std::string> process_input(const std::string & in);

	void render(uint64_t *const ts_after, const int max_wait, uint8_t **const out, int *const out_w, int *const out_h);
};
