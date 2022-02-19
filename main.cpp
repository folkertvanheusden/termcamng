#include <poll.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <thread>
#include <unistd.h>
#include <vector>
#include <netinet/in.h>
#include <netinet/tcp.h>
#include <sys/socket.h>
#include <sys/types.h>

#include "error.h"
#include "picio.h"
#include "proc.h"
#include "terminal.h"


ssize_t WRITE(int fd, const uint8_t *whereto, size_t len)
{
        ssize_t cnt = 0;

        while(len > 0) {
                ssize_t rc = write(fd, whereto, len);

                if (rc == -1)
                        return -1;
                else if (rc == 0)
                        return -1;
                else {
                        whereto += rc;
                        len -= rc;
                        cnt += rc;
                }
        }

        return cnt;
}

void send_initial_screen(const int client_fd, terminal *const t)
{
	// clear_screen, go_to 1,1
	WRITE(client_fd, reinterpret_cast<const uint8_t *>("\033[2J\033[H"), 7);

	for(int y=0; y<t->get_height(); y++) {
		for(int x=0; x<t->get_width(); x++) {
			uint8_t c = t->get_char_at(x, y);

			WRITE(client_fd, &c, 1);
		}

		WRITE(client_fd, reinterpret_cast<const uint8_t *>("\n"), 1);
	}
}

void process_program(terminal *const t, const std::string & command, const std::string & directory, const int width, const int height, const int listen_port)
{
	auto proc     = exec_with_pipe(command, directory, width, height);

	int client_fd = -1;

	// setup listening socket for viewers
	int listen_fd = socket(AF_INET, SOCK_STREAM, 0);
	if (listen_fd == -1)
		error_exit(true, "process_program: cannot create socket");

        int reuse_addr = 1;
        if (setsockopt(listen_fd, SOL_SOCKET, SO_REUSEADDR, (char *)&reuse_addr, sizeof(reuse_addr)) == -1)
                error_exit(true, "process_program: setsockopt(SO_REUSEADDR) failed");

	int q_size = SOMAXCONN;
	if (setsockopt(listen_fd, SOL_TCP, TCP_FASTOPEN, &q_size, sizeof q_size))
		error_exit(true, "process_program: failed to enable TCP FastOpen");

        struct sockaddr_in server_addr { 0 };
        server_addr.sin_family = AF_INET;
        server_addr.sin_port = htons(listen_port);

        if (bind(listen_fd, (struct sockaddr *)&server_addr, sizeof server_addr) == -1)
                error_exit(true, "process_program: failed to bind to port %d", listen_port);

        if (listen(listen_fd, q_size) == -1)
                error_exit(true, "process_program: listen failed");

        int   w_fd = std::get<1>(proc);
        int   r_fd = std::get<2>(proc);

	struct pollfd fds[] = { { listen_fd, POLLIN, 0 }, { r_fd, POLLIN, 0 }, { -1, POLLIN, 0 } };

	for(;;) {
		int rc = poll(fds, client_fd != -1 ? 3 : 2, 500);

		if (rc == 0)
			continue;

		if (rc == -1)
			error_exit(true, "process_program: poll failed");

		if (fds[0].revents) {
			if (client_fd != -1)
				close(client_fd);

			client_fd = accept(listen_fd, nullptr, nullptr);
			fds[2].fd = client_fd;

			send_initial_screen(client_fd, t);
		}

		if (fds[1].revents) {
			char buffer[4096];
			int  rc = read(r_fd, buffer, sizeof buffer);

			if (rc == -1)
				break;

			if (rc == 0)
				break;

			t->process_input(buffer, rc);

			if (WRITE(client_fd, reinterpret_cast<const uint8_t *>(buffer), rc) != rc) {
				close(client_fd);
				client_fd = -1;
			}
		}

		if (fds[2].revents) {
			char buffer[4096];
			int  rc = read(client_fd, buffer, sizeof buffer);

			if (rc == -1 || rc == 0) {
				close(client_fd);
				client_fd = -1;
			}
			else if (WRITE(w_fd, reinterpret_cast<const uint8_t *>(buffer), rc) != rc) {
				close(client_fd);
				client_fd = -1;
			}
		}
	}
}

int main(int argc, char *argv[])
{
	font f("t/FONTS/SYSTEM/FREEDOS/CPIDOS30/CP850.F16");

	const int width    = 80;
	const int height   = 25;

	const int tcp_port = 2300;

	terminal t(&f, width, height);

	std::string command   = "/usr/bin/irssi -c oftc";
	// std::string command   = "/usr/bin/httping -K 172.29.0.1";
	std::string directory = "/tmp";

	std::thread thread_handle([&t, command, directory, width, height, tcp_port] { process_program(&t, command, directory, width, height, tcp_port); });

	for(;;) {
		sleep(5);

		uint8_t *out = nullptr;
		int      out_w = 0;
		int      out_h = 0;
		t.render(&out, &out_w, &out_h);

		printf("%d x %d\n", out_w, out_h);

		FILE *fh = fopen("test.png", "w");

		write_PNG_file(fh, out_w, out_h, out);

		fclose(fh);

		free(out);
	}

	return 0;
}
