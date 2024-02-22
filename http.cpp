#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>

#include "http.h"
#include "logging.h"
#include "net-io.h"
#include "picio.h"
#include "str.h"
#include "terminal.h"


typedef enum { sct_none, sct_mjpeg, sct_mpng, sct_mbmp, sct_mtga } stream_content_type_t;

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

std::pair<uint8_t *, size_t> get_bmp_frame(terminal *const t, uint64_t *const ts_after, const int max_wait, const int compression_level)
{
	uint8_t *out   = nullptr;
	int      out_w = 0;
	int      out_h = 0;
	t->render(ts_after, max_wait, &out, &out_w, &out_h);

	uint8_t *data_out = nullptr;
	size_t data_out_len = 0;
	write_bmp(out_w, out_h, out, &data_out, &data_out_len);

	free(out);

	return { data_out, data_out_len };
}

std::pair<uint8_t *, size_t> get_tga_frame(terminal *const t, uint64_t *const ts_after, const int max_wait, const int compression_level)
{
	uint8_t *out   = nullptr;
	int      out_w = 0;
	int      out_h = 0;
	t->render(ts_after, max_wait, &out, &out_w, &out_h);

	uint8_t *data_out = nullptr;
	size_t data_out_len = 0;
	write_tga(out_w, out_h, out, &data_out, &data_out_len);

	free(out);

	return { data_out, data_out_len };
}

void get_html_root(const std::string url, net_io *const io, const void *const parameters, std::atomic_bool & stop_flag)
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

	io->send(reinterpret_cast<const uint8_t *>(reply.c_str()), reply.size());
}

void get_frame_jpeg(const std::string url, net_io *const io, const void *const parameters, std::atomic_bool & stop_flag)
{
	const http_server_parameters_t *const hsp = reinterpret_cast<const http_server_parameters_t *>(parameters);

	uint64_t after_ts = 0;
	auto     jpeg     = get_jpeg_frame(hsp->t, &after_ts, hsp->max_wait, hsp->compression_level);

	std::string reply =
			"HTTP/1.0 200 OK\r\n"
			"Content-Type: image/jpeg\r\n"
			"\r\n";

	if (io->send(reinterpret_cast<const uint8_t *>(reply.c_str()), reply.size()))
		io->send(jpeg.first, jpeg.second);

	free(jpeg.first);
}

void get_frame_png(const std::string url, net_io *const io, const void *const parameters, std::atomic_bool & stop_flag)
{
	const http_server_parameters_t *const hsp = reinterpret_cast<const http_server_parameters_t *>(parameters);

	uint64_t after_ts = 0;
	auto     png      = get_png_frame(hsp->t, &after_ts, hsp->max_wait, hsp->compression_level);

	std::string reply =
			"HTTP/1.0 200 OK\r\n"
			"Content-Type: image/png\r\n"
			"\r\n";

	if (io->send(reinterpret_cast<const uint8_t *>(reply.c_str()), reply.size()))
		io->send(png.first, png.second);

	free(png.first);
}

void get_frame_bmp(const std::string url, net_io *const io, const void *const parameters, std::atomic_bool & stop_flag)
{
	const http_server_parameters_t *const hsp = reinterpret_cast<const http_server_parameters_t *>(parameters);

	uint64_t after_ts = 0;
	auto     bmp      = get_bmp_frame(hsp->t, &after_ts, hsp->max_wait, hsp->compression_level);

	std::string reply =
			"HTTP/1.0 200 OK\r\n"
			"Content-Type: image/bmp\r\n"
			"\r\n";

	if (io->send(reinterpret_cast<const uint8_t *>(reply.c_str()), reply.size()))
		io->send(bmp.first, bmp.second);

	free(bmp.first);
}

void get_frame_tga(const std::string url, net_io *const io, const void *const parameters, std::atomic_bool & stop_flag)
{
	const http_server_parameters_t *const hsp = reinterpret_cast<const http_server_parameters_t *>(parameters);

	uint64_t after_ts = 0;
	auto     tga      = get_tga_frame(hsp->t, &after_ts, hsp->max_wait, hsp->compression_level);

	std::string reply =
			"HTTP/1.0 200 OK\r\n"
			"Content-Type: image/tga\r\n"
			"\r\n";

	if (io->send(reinterpret_cast<const uint8_t *>(reply.c_str()), reply.size()))
		io->send(tga.first, tga.second);

	free(tga.first);
}


void stream_frames(net_io *const io, const http_server_parameters_t *const parameters, const stream_content_type_t type, std::atomic_bool & stop_flag)
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

	if (io->send(reinterpret_cast<const uint8_t *>(reply.c_str()), reply.size()) == false) {
		dolog(ll_debug, "stream_frames: failed sending http headers");
		return;
	}

	uint64_t ts = 0;

	for(;!stop_flag;) {
		std::pair<uint8_t *, size_t> image;
		std::string format = "";

		if (type == sct_mpng)
			image = get_png_frame(parameters->t, &ts, parameters->max_wait, parameters->compression_level), format = "png";
		else if (type == sct_mjpeg)
			get_jpeg_frame(parameters->t, &ts, parameters->max_wait, parameters->compression_level), format = "jpeg";
		else if (type == sct_mbmp)
			image = get_bmp_frame(parameters->t, &ts, parameters->max_wait, 100), format = "bmp";
		else if (type == sct_mtga)
			image = get_tga_frame(parameters->t, &ts, parameters->max_wait, 100), format = "tga";

		std::string reply = myformat("\r\n--myboundary\r\nContent-Type: image/%s\r\nContent-Length: %zu\r\n\r\n", format.c_str(), image.second);

		if (io->send(reinterpret_cast<const uint8_t *>(reply.c_str()), reply.size()) == false) {
			dolog(ll_debug, "stream_frames: failed sending multipart http headers");

			free(image.first);

			break;
		}

		if (io->send(image.first, image.second) == false) {
			dolog(ll_debug, "stream_frames: failed sending frame data");

			free(image.first);

			break;
		}

		free(image.first);
	}
}

void get_stream(const std::string url, net_io *const io, const void *const parameters, std::atomic_bool & stop_flag)
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
	else if (extension == "mbmp")
		sct = sct_mbmp;
	else if (extension == "mtga")
		sct = sct_mtga;

	if (sct != sct_none)
		stream_frames(io, hsp, sct, stop_flag);
}

httpd * start_http_server(const std::string & bind_ip, const int http_port, http_server_parameters_t *const hsp, const std::optional<std::pair<std::string, std::string> > & tls_key_certificate)
{
	std::map<std::string, std::function<void (const std::string url, net_io *const io, const void *, std::atomic_bool & stop_flag)> > url_map;

	url_map.insert({ "/",             get_html_root });
	url_map.insert({ "/index.html",   get_html_root });
	url_map.insert({ "/frame.jpeg",   get_frame_jpeg });
	url_map.insert({ "/frame.png",    get_frame_png });
	url_map.insert({ "/frame.bmp",    get_frame_bmp });
	url_map.insert({ "/frame.tga",    get_frame_tga });
	url_map.insert({ "/stream.mjpeg", get_stream });
	url_map.insert({ "/stream.mpng",  get_stream });
	url_map.insert({ "/stream.mbmp",  get_stream });
	url_map.insert({ "/stream.mtga",  get_stream });

	return new httpd(bind_ip, http_port, url_map, hsp, tls_key_certificate);
}

void stop_http_server(httpd *const h)
{
	delete h;
}
