#include <cerrno>
#include <cstring>
#include <netdb.h>
#include <string>
#include <arpa/inet.h>
#include <netinet/in.h>
#include <netinet/tcp.h>
#include <sys/socket.h>
#include <sys/types.h>

#include "error.h"


int start_tcp_listen(const std::string & bind_to, const int listen_port)
{
	// setup listening socket for viewers
	int listen_fd = socket(AF_INET, SOCK_STREAM, 0);
	if (listen_fd == -1)
		error_exit(true, "start_tcp_listen: cannot create socket");

        int reuse_addr = 1;
        if (setsockopt(listen_fd, SOL_SOCKET, SO_REUSEADDR, (char *)&reuse_addr, sizeof(reuse_addr)) == -1)
                error_exit(true, "start_tcp_listen: setsockopt(SO_REUSEADDR) failed");

	int q_size = SOMAXCONN;
	if (setsockopt(listen_fd, SOL_TCP, TCP_FASTOPEN, &q_size, sizeof q_size))
		error_exit(true, "start_tcp_listen: failed to enable TCP FastOpen");

        struct sockaddr_in server_addr { 0 };
        server_addr.sin_family = AF_INET;
        server_addr.sin_port = htons(listen_port);
	int rc = inet_pton(AF_INET, bind_to.c_str(), &server_addr.sin_addr);

	if (rc == 0)
		error_exit(false, "start_tcp_listen: \"%s\" is not a valid IP-address", bind_to.c_str());

	if (rc == -1)
		error_exit(true, "start_tcp_listen: problem interpreting \"%s\"", bind_to.c_str());

        if (bind(listen_fd, (struct sockaddr *)&server_addr, sizeof server_addr) == -1)
                error_exit(true, "start_tcp_listen: failed to bind to port %d", listen_port);

        if (listen(listen_fd, q_size) == -1)
                error_exit(true, "start_tcp_listen: listen failed");

	return listen_fd;
}

std::string get_endpoint_name(int fd)
{
	char host[256] { "? " };
	char serv[256] { "? " };
	struct sockaddr_in6 addr { 0 };
	socklen_t addr_len = sizeof addr;

	if (getpeername(fd, (struct sockaddr *)&addr, &addr_len) == -1)
		snprintf(host, sizeof host, "[FAILED TO FIND NAME OF %d: %s (1)]", fd, strerror(errno));
	else
		getnameinfo((struct sockaddr *)&addr, addr_len, host, sizeof(host), serv, sizeof(serv), NI_NUMERICHOST | NI_NUMERICSERV);

	return std::string(host) + "." + std::string(serv);
}
