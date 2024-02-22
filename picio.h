#include <stdint.h>


void write_png(const int ncols, const int nrows, const int compression_level, const uint8_t *const in, uint8_t **const out, size_t *const out_len);
void write_jpg(const int ncols, const int nrows, const int compression_level, const uint8_t *const in, uint8_t **const out, size_t *const out_len);
void write_bmp(const int ncols, const int nrows, const int compression_level, const uint8_t *const in, uint8_t **const out, size_t *const out_len);
void write_tga(const int ncols, const int nrows, const int compression_level, const uint8_t *const in, uint8_t **const out, size_t *const out_len);
