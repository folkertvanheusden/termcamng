#include <map>
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

typedef void (*writer)(const int ncols, const int nrows, const int compression_level, const uint8_t *const in, uint8_t **const out, size_t *const out_len);

class cached_renderer
{
private:
	writer      pw       { nullptr };
	http_server_parameters_t *hsp { nullptr };
	uint64_t    ts_after { 0       };
	std::mutex  lock;
	uint8_t    *prev     { nullptr };
	size_t      prev_size{ 0       };

public:
	cached_renderer(writer pw, http_server_parameters_t *const hsp): pw(pw), hsp(hsp) {
	}

	virtual ~cached_renderer() {
		free(prev);
	}

	std::pair<uint8_t *, size_t> get_frame() {
		uint8_t *temp   = nullptr;
		int      temp_w = 0;
		int      temp_h = 0;
		if (hsp->t->render(&ts_after, hsp->max_wait, &temp, &temp_w, &temp_h, prev == nullptr)) {
			uint8_t *compressed      = nullptr;
			size_t   compressed_size = 0;
			pw(temp_w, temp_h, hsp->compression_level, temp, &compressed, &compressed_size);
			free(temp);

			std::unique_lock<std::mutex> lck(lock);
			free(prev);
			prev      = compressed;
			prev_size = compressed_size;
		}

		uint8_t *out = reinterpret_cast<uint8_t *>(malloc(prev_size));
		memcpy(out, prev, prev_size);

		return { out, prev_size };
	}
};

std::map<std::string, cached_renderer *> cr;

std::pair<uint8_t *, size_t> get_jpeg_frame()
{
	return cr.find("jpg")->second->get_frame();
}

std::pair<uint8_t *, size_t> get_png_frame()
{
	return cr.find("png")->second->get_frame();
}

std::pair<uint8_t *, size_t> get_bmp_frame()
{
	return cr.find("bmp")->second->get_frame();
}

std::pair<uint8_t *, size_t> get_tga_frame()
{
	return cr.find("tga")->second->get_frame();
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
	auto     jpeg     = get_jpeg_frame();

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
	auto     png      = get_png_frame();

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
	auto     bmp      = get_bmp_frame();

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
	auto     tga      = get_tga_frame();

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

	for(;!stop_flag;) {
		std::pair<uint8_t *, size_t> image;
		std::string format = "";

		if (type == sct_mpng)
			image = get_png_frame (), format = "png";
		else if (type == sct_mjpeg)
			image = get_jpeg_frame(), format = "jpeg";
		else if (type == sct_mbmp)
			image = get_bmp_frame (), format = "bmp";
		else if (type == sct_mtga)
			image = get_tga_frame (), format = "tga";

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

	cr.insert({ "jpg", new cached_renderer(write_jpg, hsp) });
	cr.insert({ "png", new cached_renderer(write_png, hsp) });
	cr.insert({ "bmp", new cached_renderer(write_bmp, hsp) });
	cr.insert({ "tga", new cached_renderer(write_tga, hsp) });

	return new httpd(bind_ip, http_port, url_map, hsp, tls_key_certificate);
}

void stop_http_server(httpd *const h)
{
	delete h;
}
