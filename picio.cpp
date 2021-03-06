#include <png.h>
#include <stdint.h>
#include <stdlib.h>
#include <turbojpeg.h>

#include "error.h"
#include "logging.h"
#include "picio.h"


thread_local myjpeg my_jpeg;

static void libpng_error_handler(png_structp png, png_const_charp msg)
{
	error_exit(false, "libpng error: %s", msg);
}

static void libpng_warning_handler(png_structp png, png_const_charp msg)
{
	printf("libpng warning: %s\n", msg);
}

void write_PNG_file(FILE *const fh, const int ncols, const int nrows, const int compression_level, uint8_t *pixels)
{
	png_bytep *row_pointers = (png_bytep *)malloc(sizeof(png_bytep) * nrows);
	if (!row_pointers)
		error_exit(true, "write_PNG_file error allocating row-pointers");

	for(int y=0; y<nrows; y++)
		row_pointers[y] = &pixels[y*ncols*3];

	png_structp png = png_create_write_struct(PNG_LIBPNG_VER_STRING, NULL, libpng_error_handler, libpng_warning_handler);
	if (!png)
		error_exit(false, "png_create_write_struct failed");

	png_infop info = png_create_info_struct(png);
	if (info == NULL)
		error_exit(false, "png_create_info_struct failed");

	png_init_io(png, fh);

	png_set_compression_level(png, compression_level * 9 / 100);

	png_set_IHDR(png, info, ncols, nrows, 8, PNG_COLOR_TYPE_RGB, PNG_INTERLACE_NONE, PNG_COMPRESSION_TYPE_DEFAULT, PNG_FILTER_TYPE_DEFAULT);

	png_text text_ptr[2];
	text_ptr[0].key = (png_charp)"Author";
	text_ptr[0].text = (png_charp)"termcamng";
	text_ptr[0].compression = PNG_TEXT_COMPRESSION_NONE;
	text_ptr[1].key = (png_charp)"URL";
	text_ptr[1].text = (png_charp)"https://github.com/folkertvanheusden/termcamng";
	text_ptr[1].compression = PNG_TEXT_COMPRESSION_NONE;
	png_set_text(png, info, text_ptr, 2);

	png_write_info(png, info);

	png_write_image(png, row_pointers);

	png_write_end(png, NULL);

	png_destroy_write_struct(&png, &info);

	free(row_pointers);
}

myjpeg::myjpeg()
{
	jpegCompressor = tjInitCompress();
}

myjpeg::~myjpeg()
{
	tjDestroy(jpegCompressor);
}

bool myjpeg::write_JPEG_memory(const int ncols, const int nrows, const int compression_level, const uint8_t *const pixels, uint8_t **out, size_t *out_len)
{
	unsigned long int len = 0;

	if (tjCompress2(jpegCompressor, pixels, ncols, 0, nrows, TJPF_RGB, out, &len, TJSAMP_444, 100 - compression_level, TJFLAG_FASTDCT) == -1) {
		dolog(ll_error, "Failed compressing frame: %s (%dx%d @ %d)", tjGetErrorStr(), ncols, nrows, compression_level);
		return false;
	}

	*out_len = len;

	return true;
}
