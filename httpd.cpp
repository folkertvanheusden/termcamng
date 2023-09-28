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
#include "str.h"


httpd::httpd(const std::string & bind_interface, const int bind_port, const std::map<std::string, std::function<void (const std::string, const int fd, const void *, std::atomic_bool & stop_flag)> > & url_map, const void *const parameters) :
	url_map(url_map),
	parameters(parameters)
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

void httpd::handle_request(const int fd)
{
	std::string request_headers;

	while(request_headers.find("\r\n\r\n") == std::string::npos) {
		char buffer[4096];

		int rrc = read(fd, buffer, sizeof buffer);
		if (rrc == 0)  // connection close before request headers have been received
			return;

		if (rrc == -1) {
			dolog(ll_info, "httpd::handle_request: read failed: %s", strerror(errno));
			break;
		}

		request_headers += std::string(buffer, rrc);
	}

	auto request_lines = split(request_headers, "\r\n");
	if (request_lines.size() == 0)
		return;

	auto request = split(request_lines.at(0), " ");
	if (request.size() != 3)
		return;

	if (request.at(0) == "HEAD") {
		std::string reply = "HTTP/1.0 200 OK\r\n";

		WRITE(fd, reinterpret_cast<const uint8_t *>(reply.c_str()), reply.size());

		return;
	}

	if (request.at(0) != "GET")
		return;

	auto it = url_map.find(request.at(1));

	if (it == url_map.end()) {
		std::string reply = "HTTP/1.0 404 OK\r\n";

		WRITE(fd, reinterpret_cast<const uint8_t *>(reply.c_str()), reply.size());

		return;
	}

	it->second(request.at(1), fd, parameters, stop_flag);
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

		if (client_fd == -1)
			dolog(ll_info, "httpd::operator: accept failed: %s", strerror(errno));
		else {
			std::thread request_handler([this, client_fd] {
					handle_request(client_fd);
					close(client_fd);
					});

			request_handler.detach();
		}
	}
}
