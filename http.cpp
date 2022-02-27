#include <microhttpd.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>

#include "http.h"
#include "picio.h"
#include "terminal.h"


typedef struct
{
	terminal *t;

	bool      png;

	int       compression_level;

	uint64_t  buffer_ts;

	uint8_t  *buffer;
	size_t    bytes_in_buffer;
} http_parameters_t;

void *free_parameters(void *cls)
{
	http_parameters_t *p = reinterpret_cast<http_parameters_t *>(cls);

	free(p->buffer);
	free(p);

	return nullptr;
}

std::pair<uint8_t *, size_t> get_jpeg_frame(terminal *const t, uint64_t *const ts_after, const int compression_level)
{
	uint8_t *out   = nullptr;
	int      out_w = 0;
	int      out_h = 0;
	t->render(ts_after, &out, &out_w, &out_h);

	uint8_t *data_out = nullptr;
	size_t   data_out_len = 0;

	(void)my_jpeg.write_JPEG_memory(out_w, out_h, compression_level, out, &data_out, &data_out_len);

	free(out);

	return { data_out, data_out_len };
}

std::pair<uint8_t *, size_t> get_png_frame(terminal *const t, uint64_t *const ts_after, const int compression_level)
{
	uint8_t *out   = nullptr;
	int      out_w = 0;
	int      out_h = 0;
	t->render(ts_after, &out, &out_w, &out_h);

	char *data_out = nullptr;
	size_t data_out_len = 0;

	FILE *fh = open_memstream(&data_out, &data_out_len);

	write_PNG_file(fh, out_w, out_h, compression_level, out);

	fclose(fh);

	free(out);

	return { reinterpret_cast<uint8_t *>(data_out), data_out_len };
}

ssize_t stream_producer(void *cls, uint64_t pos, char *buf, size_t max)
{
	http_parameters_t *p = reinterpret_cast<http_parameters_t *>(cls);

	if (p->buffer == nullptr) {
		auto image = p->png ? get_png_frame(p->t, &p->buffer_ts, p->compression_level) : get_jpeg_frame(p->t, &p->buffer_ts, p->compression_level);

		int header_len = asprintf(reinterpret_cast<char **>(&p->buffer), "--12345\r\nContent-Type: image/%s\r\nContent-Length: %zu\r\n\r\n", p->png ? "png" : "jpeg", image.second);

		uint8_t *temp = reinterpret_cast<uint8_t *>(realloc(p->buffer, header_len + image.second));
		if (!temp)
			return 0;

		p->buffer = temp;

		memcpy(&p->buffer[header_len], image.first, image.second);

		p->bytes_in_buffer = header_len + image.second;

		free(image.first);
	}

	size_t n_to_copy = std::min(max, p->bytes_in_buffer);

	memcpy(buf, p->buffer, n_to_copy);

	size_t left = p->bytes_in_buffer - n_to_copy;
	if (left > 0) {
		memmove(&p->buffer[0], &p->buffer[n_to_copy], left);

		p->bytes_in_buffer -= left;
	}
	else {
		free(p->buffer);
		p->buffer = nullptr;

		p->bytes_in_buffer = 0;
	}

	return n_to_copy;
}

MHD_Result get_terminal_png_frame(void *cls,
		struct MHD_Connection *connection,
		const char *url,
		const char *method,
		const char *version,
		const char *upload_data, size_t *upload_data_size, void **ptr)
{
	if (strcmp(method, "GET") != 0)
		return MHD_NO;

	http_server_parameters_t *const hsp = reinterpret_cast<http_server_parameters_t *>(cls);

	if (strcmp(url, "/") == 0) {
		uint64_t after_ts = 0;
		auto     png      = get_png_frame(hsp->t, &after_ts, hsp->compression_level);

		struct MHD_Response *response = MHD_create_response_from_buffer(png.second, png.first, MHD_RESPMEM_MUST_COPY);

		free(png.first);

		MHD_Result ret = MHD_NO;

		if (MHD_add_response_header(response, MHD_HTTP_HEADER_CONTENT_TYPE, "image/png") == MHD_NO)
			ret = MHD_NO;
		else
			ret = MHD_queue_response(connection, MHD_HTTP_OK, response);

		MHD_destroy_response(response);

		return ret;
	}

	if (strcmp(url, "/stream") == 0  || strcmp(url, "/stream.mjpeg") == 0) {
		http_parameters_t *parameters = reinterpret_cast<http_parameters_t *>(calloc(1, sizeof(http_parameters_t)));
		if (!parameters)
			return MHD_NO;

		parameters->t                 = hsp->t;

		parameters->png               = strcmp(url, "/stream") == 0;

		parameters->compression_level = hsp->compression_level;

		struct MHD_Response *response = MHD_create_response_from_callback(MHD_SIZE_UNKNOWN, hsp->t->get_width() * hsp->t->get_height() * 3 * 2, &stream_producer, parameters, reinterpret_cast<MHD_ContentReaderFreeCallback>(free_parameters));

		MHD_Result ret = MHD_YES;

		if (MHD_add_response_header(response, MHD_HTTP_HEADER_CONTENT_TYPE, "multipart/x-mixed-replace; boundary=--12345") == MHD_NO)
			ret = MHD_NO;
		else
			ret = MHD_queue_response(connection, MHD_HTTP_OK, response);

		MHD_destroy_response(response);

		return ret;
	}

	return MHD_NO;
}

struct MHD_Daemon * start_http_server(const int http_port, http_server_parameters_t *const hsp)
{
	return MHD_start_daemon(
			MHD_USE_SELECT_INTERNALLY | MHD_USE_DEBUG,
			http_port,
			nullptr, nullptr, &get_terminal_png_frame, reinterpret_cast<void *>(hsp),
			MHD_OPTION_CONNECTION_TIMEOUT, (unsigned int) 120,
			MHD_OPTION_END);
}

void stop_http_server(struct MHD_Daemon *const d)
{
	MHD_stop_daemon(d);
}
