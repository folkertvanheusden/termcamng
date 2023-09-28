#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>

#include "http.h"
#include "io.h"
#include "picio.h"
#include "str.h"
#include "terminal.h"


typedef enum { sct_none, sct_mjpeg, sct_mpng } stream_content_type_t;

std::pair<uint8_t *, size_t> get_jpeg_frame(terminal *const t, uint64_t *const ts_after, const int max_wait, const int compression_level)
{
	uint8_t *out   = nullptr;
	int      out_w = 0;
	int      out_h = 0;
	t->render(ts_after, max_wait, &out, &out_w, &out_h);

	uint8_t *data_out = nullptr;
	size_t   data_out_len = 0;

	(void)my_jpeg.write_JPEG_memory(out_w, out_h, compression_level, out, &data_out, &data_out_len);

	free(out);

	return { data_out, data_out_len };
}

std::pair<uint8_t *, size_t> get_png_frame(terminal *const t, uint64_t *const ts_after, const int max_wait, const int compression_level)
{
	uint8_t *out   = nullptr;
	int      out_w = 0;
	int      out_h = 0;
	t->render(ts_after, max_wait, &out, &out_w, &out_h);

	char *data_out = nullptr;
	size_t data_out_len = 0;

	FILE *fh = open_memstream(&data_out, &data_out_len);

	write_PNG_file(fh, out_w, out_h, compression_level, out);

	fclose(fh);

	free(out);

	return { reinterpret_cast<uint8_t *>(data_out), data_out_len };
}

void get_html_root(const std::string url, const int fd, const void *const parameters, std::atomic_bool & stop_flag)
{
	std::string reply = 
			"HTTP/1.0 200 OK\r\n"
			"Content-Type: text/html\r\n"
			"\r\n"
			"<!DOCTYPE html>"
			"<html lang=\"en\">"
			"<body>"
			"<img src=\"/stream.mjpeg\">"
			"</body>"
			"</html>";

	WRITE(fd, reinterpret_cast<const uint8_t *>(reply.c_str()), reply.size());
}

void get_frame_jpeg(const std::string url, const int fd, const void *const parameters, std::atomic_bool & stop_flag)
{
	const http_server_parameters_t *const hsp = reinterpret_cast<const http_server_parameters_t *>(parameters);

	uint64_t after_ts = 0;
	auto     jpeg     = get_jpeg_frame(hsp->t, &after_ts, hsp->max_wait, hsp->compression_level);

	std::string reply =
			"HTTP/1.0 200 OK\r\n"
			"Content-Type: image/jpeg\r\n"
			"\r\n";

	if (size_t(WRITE(fd, reinterpret_cast<const uint8_t *>(reply.c_str()), reply.size())) == reply.size())
		WRITE(fd, jpeg.first, jpeg.second);

	free(jpeg.first);
}

void get_frame_png(const std::string url, const int fd, const void *const parameters, std::atomic_bool & stop_flag)
{
	const http_server_parameters_t *const hsp = reinterpret_cast<const http_server_parameters_t *>(parameters);

	uint64_t after_ts = 0;
	auto     png      = get_png_frame(hsp->t, &after_ts, hsp->max_wait, hsp->compression_level);

	std::string reply =
			"HTTP/1.0 200 OK\r\n"
			"Content-Type: image/png\r\n"
			"\r\n";

	if (size_t(WRITE(fd, reinterpret_cast<const uint8_t *>(reply.c_str()), reply.size())) == reply.size())
		WRITE(fd, png.first, png.second);

	free(png.first);
}

void stream_frames(const int fd, const http_server_parameters_t *const parameters, const stream_content_type_t type, std::atomic_bool & stop_flag)
{
	std::string reply =
		"HTTP/1.0 200 OK\r\n"
		"Cache-Control: no-cache\r\n"
		"Pragma: no-cache\r\n"
		"Server: TermCamNG\r\n"
		"Expires: Thu, 01 Dec 1994 16:00:00 GMT\r\n"
		"Connection: close\r\n"
		"Content-Type: multipart/x-mixed-replace; boundary=myboundary\r\n"
		"\r\n";

	if (size_t(WRITE(fd, reinterpret_cast<const uint8_t *>(reply.c_str()), reply.size())) != reply.size())
		return;

	uint64_t ts = 0;

	for(;!stop_flag;) {
		auto image = type == sct_mpng ? get_png_frame(parameters->t, &ts, parameters->max_wait, parameters->compression_level) : get_jpeg_frame(parameters->t, &ts, parameters->max_wait, parameters->compression_level);

		std::string reply = myformat("\r\n--myboundary\r\nContent-Type: image/%s\r\nContent-Length: %zu\r\n\r\n", type == sct_mpng ? "png" : "jpeg", image.second);

		if (size_t(WRITE(fd, reinterpret_cast<const uint8_t *>(reply.c_str()), reply.size())) != reply.size()) {
			free(image.first);

			break;
		}

		if (size_t(WRITE(fd, image.first, image.second)) != image.second) {
			free(image.first);

			break;
		}

		free(image.first);
	}
}

void get_stream(const std::string url, const int fd, const void *const parameters, std::atomic_bool & stop_flag)
{
	const http_server_parameters_t *const hsp = reinterpret_cast<const http_server_parameters_t *>(parameters);

	std::size_t dot = url.rfind('.');

	if (dot == std::string::npos)
		return;

	const std::string extension = url.substr(dot + 1);

	stream_content_type_t sct = sct_none;

	if (extension == "mjpeg")
		sct = sct_mjpeg;
	else if (extension == "mpng")
		sct = sct_mpng;

	if (sct != sct_none)
		stream_frames(fd, hsp, sct, stop_flag);
}

httpd * start_http_server(const std::string & bind_ip, const int http_port, http_server_parameters_t *const hsp)
{
	std::map<std::string, std::function<void (const std::string url, const int fd, const void *, std::atomic_bool & stop_flag)> > url_map;

	url_map.insert({ "/",             get_html_root });
	url_map.insert({ "/index.html",   get_html_root });
	url_map.insert({ "/frame.jpeg",   get_frame_jpeg });
	url_map.insert({ "/frame.png",    get_frame_png });
	url_map.insert({ "/stream.mjpeg", get_stream });
	url_map.insert({ "/stream.mpng",  get_stream });

	return new httpd(bind_ip, http_port, url_map, hsp);
}

void stop_http_server(httpd *const h)
{
	delete h;
}
