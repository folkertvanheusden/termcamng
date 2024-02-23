#include <errno.h>
#include <optional>
#include <poll.h>
#include <string>
#include <string.h>
#include <unistd.h>
#include <sys/socket.h>
#include <sys/types.h>

#include "error.h"
#include "httpd.h"
#include "io.h"
#include "logging.h"
#include "net.h"
#include "net-io-wolfssl.h"
#include "net-io-fd.h"
#include "str.h"
#include "time.h"


httpd::httpd(const std::string & bind_interface, const int bind_port, const std::map<std::string, std::function<void (const std::string, net_io *const io, const void *, std::atomic_bool & stop_flag, const bool peek)> > & url_map, const void *const parameters, const std::optional<std::pair<std::string, std::string> > tls_key_certificate) :
	url_map(url_map),
	parameters(parameters),
	tls_key_certificate(tls_key_certificate)
{
	server_fd = start_tcp_listen(bind_interface, bind_port);

	th = new std::thread(std::ref(*this));
}

httpd::~httpd()
{
	stop_flag = true;

	th->join();
	delete th;

	close(server_fd);
}

bool httpd::handle_request(net_io *const io, const std::string & endpoint)
{
	std::string request_headers;

	while(request_headers.find("\r\n\r\n") == std::string::npos) {
		char buffer[4096];

		int rrc = io->read(reinterpret_cast<uint8_t *>(buffer), sizeof buffer);
		if (rrc == 0) {  // connection close before request headers have been received
			dolog(ll_info, "httpd::handle_request: connection closed");
			return false;
		}

		if (rrc == -1) {
			dolog(ll_info, "httpd::handle_request: read failed");
			return false;
		}

		request_headers += std::string(buffer, rrc);
	}

	auto request_lines = split(request_headers, "\r\n");
	if (request_lines.size() == 0) {
		request_lines = split(request_headers, "\n");

		if (request_lines.size() == 0) {
			dolog(ll_info, "httpd::handle_request: end of request headers not found");
			return false;
		}
	}

	auto request = split(request_lines.at(0), " ");
	if (request.size() < 3) {
		dolog(ll_info, "httpd::handle_request: request line malformed");
		return false;
	}

	if (request.at(0) != "GET" && request.at(0) != "HEAD") {
		dolog(ll_info, "httpd::handle_request: not a GET/HEAD request");
		return false;
	}

	auto it = url_map.find(request.at(1));

	if (it == url_map.end()) {
		std::string reply = "HTTP/1.0 404 OK\r\n";

		dolog(ll_debug, "httpd::handle_request(%s): url %s not found", endpoint.c_str(), request.at(1).c_str());

		io->send(reinterpret_cast<const uint8_t *>(reply.c_str()), reply.size());

		return false;
	}

	dolog(ll_info, "httpd::handle_request(%s): requested url %s", endpoint.c_str(), request.at(1).c_str());

	it->second(request.at(1), io, parameters, stop_flag, request.at(0) == "HEAD");

	return true;
}

void httpd::operator()()
{
	struct pollfd fds[] { { server_fd, POLLIN, 0 } };

	for(;!stop_flag;) {
		int prc = poll(fds, 1, 500);

		if (prc == 0)
			continue;

		if (prc == -1)
			error_exit(true, "httpd::operator: poll failed");

		int client_fd = accept(server_fd, nullptr, nullptr);

		if (client_fd == -1) {
			dolog(ll_info, "httpd::operator: accept failed: %s", strerror(errno));
			continue;
		}

		std::thread request_handler([this, client_fd] {
				auto  connection_start = get_ms();

				std::string endpoint = get_endpoint_name(client_fd);

				dolog(ll_info, "httpd::operator: connected with: %s", endpoint.c_str());

				net_io *io = nullptr;

				if (tls_key_certificate.has_value())
					io = new net_io_wolfssl(client_fd, tls_key_certificate->first, tls_key_certificate->second);
				else
					io = new net_io_fd(client_fd);

				while(handle_request(io, endpoint) == true) {
					// HTTP/1.1
				}

				delete io;

				close(client_fd);

				dolog(ll_info, "httpd::operator: disconnected from: %s (took: %.3f seconds)", endpoint.c_str(), (get_ms() - connection_start) / 1000.);
			});

		request_handler.detach();
	}
}
