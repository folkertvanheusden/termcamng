#include <assert.h>
#include <png.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>
#include <turbojpeg.h>

#include "error.h"
#include "logging.h"
#include "picio.h"


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

class myjpeg
{
private:
       tjhandle jpegCompressor;

public:
       myjpeg();
       virtual ~myjpeg();

       bool write_JPEG_memory(const int ncols, const int nrows, const int compression_level, const uint8_t *const pixels, uint8_t **out, size_t *out_len);
};

thread_local myjpeg my_jpeg;

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

void write_png(const int ncols, const int nrows, const int compression_level, const uint8_t *const in, uint8_t **const out, size_t *const out_len)
{
	FILE *fh = open_memstream(reinterpret_cast<char **>(out), out_len);
	write_PNG_file(fh, ncols, nrows, compression_level, const_cast<uint8_t *>(in));
	fclose(fh);
}

void write_jpg(const int ncols, const int nrows, const int compression_level, const uint8_t *const in, uint8_t **const out, size_t *const out_len)
{
	my_jpeg.write_JPEG_memory(ncols, nrows, compression_level, in, out, out_len);
}

void write_bmp(const int ncols, const int nrows, const int compression_level, const uint8_t *const in, uint8_t **const out, size_t *const out_len)
{
	*out_len = ncols * nrows * 3 + 2 + 12 + 40;
	*out = new uint8_t[*out_len];

	size_t offset = 0;
	(*out)[offset++] = 'B';
	(*out)[offset++] = 'M';
	(*out)[offset++] = *out_len;  // file size in bytes
	(*out)[offset++] = *out_len >> 8;
	(*out)[offset++] = *out_len >> 16;
	(*out)[offset++] = *out_len >> 24;
	(*out)[offset++] = 0x00;  // reserved
	(*out)[offset++] = 0x00;
	(*out)[offset++] = 0x00;
	(*out)[offset++] = 0x00;
	(*out)[offset++] = 54;  // offset of start (2 + 12 + 40)
	(*out)[offset++] = 0x00;
	(*out)[offset++] = 0x00;
	(*out)[offset++] = 0x00;
	assert(offset == 0x0e);
	(*out)[offset++] = 40;  // header size
	(*out)[offset++] = 0x00;
	(*out)[offset++] = 0x00;
	(*out)[offset++] = 0x00;
	(*out)[offset++] = ncols;
	(*out)[offset++] = ncols >> 8;
	(*out)[offset++] = ncols >> 16;
	(*out)[offset++] = 0x00;
	(*out)[offset++] = nrows;
	(*out)[offset++] = nrows >> 8;
	(*out)[offset++] = nrows >> 16;
	(*out)[offset++] = 0x00;
	(*out)[offset++] = 0x01;  // color planes
	(*out)[offset++] = 0x00;
	(*out)[offset++] = 24;  // bits per pixel
	(*out)[offset++] = 0x00;
	(*out)[offset++] = 0x00;  // compression method
	(*out)[offset++] = 0x00;
	(*out)[offset++] = 0x00;
	(*out)[offset++] = 0x00;
	(*out)[offset++] = 0x00;  // image size
	(*out)[offset++] = 0x00;
	(*out)[offset++] = 0x00;
	(*out)[offset++] = 0x00;
	(*out)[offset++] = ncols;
	(*out)[offset++] = ncols >> 8;
	(*out)[offset++] = ncols >> 16;
	(*out)[offset++] = 0x00;
	(*out)[offset++] = nrows;
	(*out)[offset++] = nrows >> 8;
	(*out)[offset++] = nrows >> 16;
	(*out)[offset++] = 0x00;
	(*out)[offset++] = 0x00;  // color count
	(*out)[offset++] = 0x00;
	(*out)[offset++] = 0x00;
	(*out)[offset++] = 0x00;
	(*out)[offset++] = 0x00;  // important colors
	(*out)[offset++] = 0x00;
	(*out)[offset++] = 0x00;
	(*out)[offset++] = 0x00;
	assert(offset == 40 + 12 + 2);

	for(int y=nrows - 1; y >= 0; y--) {
		size_t in_o = y * ncols * 3;
		for(int x=0; x<ncols; x++) {
			size_t in_o2 = in_o + x * 3;
			(*out)[offset++] = in[in_o2 + 2];
			(*out)[offset++] = in[in_o2 + 1];
			(*out)[offset++] = in[in_o2 + 0];
		}
	}
}

void write_tga(const int ncols, const int nrows, const int compression_level, const uint8_t *const in, uint8_t **const out, size_t *const out_len)
{
	*out_len = ncols * nrows * 3 + 18;
	*out = new uint8_t[*out_len];

	size_t offset = 0;
	(*out)[offset++] = 0;  // id length
	(*out)[offset++] = 0;  // colormap (none)
	(*out)[offset++] = 2;  // image type: uncompressed truecolor
	offset += 5;  // color map specification
	(*out)[offset++] = 0;  // x origin
	(*out)[offset++] = 0;
	(*out)[offset++] = 0;  // y origin
	(*out)[offset++] = 0;
	(*out)[offset++] = ncols;
	(*out)[offset++] = ncols >> 8;  // width
	(*out)[offset++] = nrows;
	(*out)[offset++] = nrows >> 8;  // height
	(*out)[offset++] = 24;  // bit per pixel
	(*out)[offset++] = 32;  // top to bottom
	for(size_t i=0; i<nrows * ncols * 3; i += 3) {
		(*out)[offset++] = in[i + 18 + 2];
		(*out)[offset++] = in[i + 18 + 1];
		(*out)[offset++] = in[i + 18 + 0];
	}
}

void write_simple(const int ncols, const int nrows, const int compression_level, const uint8_t *const in, uint8_t **const out, size_t *const out_len)
{
	*out_len = ncols * nrows * 3 + 4;
	*out = new uint8_t[*out_len];

	size_t offset = 0;
	(*out)[offset++] = ncols >> 8;  // width
	(*out)[offset++] = ncols;
	(*out)[offset++] = nrows >> 8;  // height
	(*out)[offset++] = nrows;
	memcpy(&(*out)[offset++], in, ncols * nrows * 3);
}
