#include <stdio.h>
#include <turbojpeg.h>


void write_PNG_file(FILE *const fh, const int ncols, const int nrows, const int compression_level, uint8_t *pixels);

class myjpeg
{
private:
	tjhandle jpegCompressor;

public:
	myjpeg();
	virtual ~myjpeg();

	bool write_JPEG_memory(const int ncols, const int nrows, const int compression_level, const uint8_t *const pixels, uint8_t **out, size_t *out_len);
};

extern thread_local myjpeg my_jpeg;
