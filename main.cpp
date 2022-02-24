#include <assert.h>
#include <atomic>
#include <microhttpd.h>
#include <poll.h>
#include <signal.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <thread>
#include <unistd.h>
#include <vector>
#include <arpa/inet.h>
#include <libssh/libssh.h>
#include <libssh/server.h>
#include <netinet/in.h>
#include <netinet/tcp.h>
#include <security/pam_appl.h>
#include <security/pam_misc.h>
#include <sys/socket.h>
#include <sys/types.h>

#include "error.h"
#include "io.h"
#include "logging.h"
#include "picio.h"
#include "proc.h"
#include "str.h"
#include "terminal.h"
#include "utils.h"
#include "yaml-helpers.h"


std::atomic_bool stop { false };

void signal_handler(int sig)
{
	stop = true;
}

std::string generate_initial_screen(terminal *const t)
{
	std::string out;

	// clear_screen, go_to 1,1
	out += "\033[2J";

	int w = t->get_width();
	int h = t->get_height();

	for(int y=0; y<h; y++) {
		bool last_line = y == h - 1;

		int  cur_w     = last_line ? w - 1 : w;

		out += myformat("\033[%dH", y + 1);

		for(int x=0; x<cur_w; x++) {
			pos_t c = t->get_cell_at(x, y);

			out += myformat("\033[%d;%dm%c", c.fg_col_ansi, c.bg_col_ansi, c.c);
		}
	}

	auto cursor_location = t->get_current_xy();

	out += myformat("\033[%d;%dH", cursor_location.second + 1, cursor_location.first + 1);

	return out;
}

std::string setup_telnet_session()
{
	uint8_t dont_auth[]        = { 0xff, 0xf4, 0x25 };
	uint8_t suppress_goahead[] = { 0xff, 0xfb, 0x03 };
	uint8_t dont_linemode[]    = { 0xff, 0xfe, 0x22 };
	uint8_t dont_new_env[]     = { 0xff, 0xfe, 0x27 };
	uint8_t will_echo[]        = { 0xff, 0xfb, 0x01 };
	uint8_t dont_echo[]        = { 0xff, 0xfe, 0x01 };
	uint8_t noecho[]           = { 0xff, 0xfd, 0x2d };

	std::string out;

	out += std::string(reinterpret_cast<const char *>(dont_auth),        3);
	out += std::string(reinterpret_cast<const char *>(suppress_goahead), 3);
	out += std::string(reinterpret_cast<const char *>(dont_linemode),    3);
	out += std::string(reinterpret_cast<const char *>(dont_new_env),     3);
	out += std::string(reinterpret_cast<const char *>(will_echo),        3);
	out += std::string(reinterpret_cast<const char *>(dont_echo),        3);
	out += std::string(reinterpret_cast<const char *>(noecho),           3);

	return out;
}

int function_conversation(int num_msg, const struct pam_message **msg, struct pam_response **resp, void *appdata_ptr)
{
	pam_response *reply = (pam_response *)appdata_ptr;

	*resp = reply;

	return PAM_SUCCESS;
}

std::pair<bool, std::string> authenticate_against_pam(const std::string & username, const std::string & password)
{
	struct pam_response *reply = (struct pam_response *)malloc(sizeof(struct pam_response));
	reply->resp = strdup(password.c_str());
	reply->resp_retcode = 0;

	const struct pam_conv local_conversation { function_conversation, reply };

	pam_handle_t *local_auth_handle = nullptr;
	if (pam_start("common-auth", username.c_str(), &local_conversation, &local_auth_handle) != PAM_SUCCESS) {
		free(reply);
		return { false, "PAM error" };
	}

	int rc = pam_authenticate(local_auth_handle, PAM_SILENT);
	pam_end(local_auth_handle, rc);

	if (rc == PAM_AUTH_ERR)
		return { false, "Authentication failed" };

	if (rc != PAM_SUCCESS)
		return { false, "PAM error" };

	return { true, "Ok" };
}

typedef struct
{
	std::mutex               lock;
	std::vector<std::string> queue;
} client_t;

typedef struct
{
	std::mutex                        lock;
	std::map<std::string, client_t *> clients;
} clients_t;

void process_ssh(terminal *const t, const std::string & ssh_keys, const std::string & bind_addr, const int port, const int program_fd, clients_t *const clients)
{
	// setup ssh server
	ssh_bind sshbind = ssh_bind_new();

	ssh_bind_options_set(sshbind, SSH_BIND_OPTIONS_RSAKEY, myformat("%s/ssh_host_rsa_key", ssh_keys.c_str()).c_str());

	ssh_bind_options_set(sshbind, SSH_BIND_OPTIONS_BINDADDR, bind_addr.c_str());

	ssh_bind_options_set(sshbind, SSH_BIND_OPTIONS_BINDPORT, &port);

	if (ssh_bind_listen(sshbind) < 0)
		error_exit(false, "process_ssh: error listening to socket: %s", ssh_get_error(sshbind));

	for(;!stop;) {
		bool err = false;

		// wait for client
		ssh_session session = ssh_new();

		int r = ssh_bind_accept(sshbind, session);  // TODO unblock
		if (r == SSH_ERROR) {
			dolog(ll_warning, "process_ssh: error accepting a connection: %s", ssh_get_error(sshbind));
			ssh_disconnect(session);
			ssh_free(session);
			continue;
		}

		// authenticate
		if (ssh_handle_key_exchange(session)) {
			dolog(ll_warning, "process_ssh: ssh_handle_key_exchange: %s", ssh_get_error(session));
			ssh_disconnect(session);
			ssh_free(session);
			continue;
		}

		bool        auth_success = false;
		std::string username;

		while(!stop && !auth_success) {
			ssh_message message = ssh_message_get(session);
			if (!message)
				break;

			ssh_message_auth_set_methods(message, SSH_AUTH_METHOD_PASSWORD);

			if (ssh_message_type(message) == SSH_REQUEST_AUTH && ssh_message_subtype(message) == SSH_AUTH_METHOD_PASSWORD) {
				const char *requested_user = ssh_message_auth_user(message);

				auto auth_rc =  authenticate_against_pam(requested_user, ssh_message_auth_password(message));

				if (auth_rc.first) {
					auth_success = true;

					username = requested_user;

					ssh_message_auth_reply_success(message, 0);

					ssh_message_free(message);
					break;
				}
			}

			ssh_message_reply_default(message);

			ssh_message_free(message);
		}

		if (!auth_success) {
			ssh_disconnect(session);
			ssh_free(session);
			continue;
		}

		// select a channel
		ssh_channel channel = nullptr;
		while(!stop && channel == nullptr) {
			ssh_message message = ssh_message_get(session);

			if (!message) {
				err = true;
				break;
			}

			if (ssh_message_type(message) == SSH_REQUEST_CHANNEL_OPEN) {
				if (ssh_message_subtype(message) == SSH_CHANNEL_SESSION)
					channel = ssh_message_channel_request_open_reply_accept(message);
			}

			ssh_message_free(message);
		}

		if (err) {
			ssh_disconnect(session);
			ssh_free(session);
			continue;
		}

		// wait for shell request
		while(!stop) {
			ssh_message message = ssh_message_get(session);
			if (!message)
				break;

			if (ssh_message_type(message) == SSH_REQUEST_CHANNEL && ssh_message_subtype(message) == SSH_CHANNEL_REQUEST_SHELL) {
				ssh_message_channel_request_reply_success(message);
				ssh_message_free(message);
				break;
			}

			ssh_message_free(message);
		}

		std::thread client_thread([session, username, channel, clients, t, program_fd] {
			set_thread_name("ssh-" + username);

			std::string user_key = username + "_ssh_" + myformat("%d", ssh_get_fd(session));

			std::unique_lock<std::mutex> lck(clients->lock);

			client_t *client = new client_t();
			auto it_client = clients->clients.insert({ user_key, client });

			lck.unlock();

			std::string setup   = setup_telnet_session();

			std::string data    = generate_initial_screen(t);

			std::string initial = setup + data;

			ssh_channel_write(channel, initial.c_str(), initial.size());

			while(!stop && !ssh_channel_is_eof(channel)) {
				// read from ssh, transmit to terminal
				char buffer[4096];

				int i = ssh_channel_read_timeout(channel, buffer, sizeof buffer, 0, 50);

				if (i > 0) {
					if (WRITE(program_fd, reinterpret_cast<const uint8_t *>(buffer), i) != i)
						break;
				}

				// transmit any queued traffic
				std::unique_lock<std::mutex> lck(it_client.first->second->lock);
				while(!it_client.first->second->queue.empty()) {
					std::string data = it_client.first->second->queue.at(0);

					it_client.first->second->queue.erase(it_client.first->second->queue.begin() + 0);

					ssh_channel_write(channel, data.c_str(), data.size());
				}

				lck.unlock();
			}

			ssh_disconnect(session);
			ssh_free(session);

			// unregister client from queues
			lck.lock();

			clients->clients.erase(user_key);

			delete client;
		});

		client_thread.detach();
	}

	ssh_finalize();

	printf("process_ssh: thread terminating\n");
}

void process_telnet(terminal *const t, const int program_fd, const int width, const int height, const std::string & bind_to, const int listen_port, clients_t *const clients)
{
	int client_fd = -1;

	// setup listening socket for viewers
	int listen_fd = socket(AF_INET, SOCK_STREAM, 0);
	if (listen_fd == -1)
		error_exit(true, "process_telnet: cannot create socket");

        int reuse_addr = 1;
        if (setsockopt(listen_fd, SOL_SOCKET, SO_REUSEADDR, (char *)&reuse_addr, sizeof(reuse_addr)) == -1)
                error_exit(true, "process_telnet: setsockopt(SO_REUSEADDR) failed");

	int q_size = SOMAXCONN;
	if (setsockopt(listen_fd, SOL_TCP, TCP_FASTOPEN, &q_size, sizeof q_size))
		error_exit(true, "process_telnet: failed to enable TCP FastOpen");

        struct sockaddr_in server_addr { 0 };
        server_addr.sin_family = AF_INET;
        server_addr.sin_port = htons(listen_port);
	int rc = inet_pton(AF_INET, bind_to.c_str(), &server_addr.sin_addr);

	if (rc == 0)
		error_exit(false, "process_telnet: \"%s\" is not a valid IP-address", bind_to.c_str());

	if (rc == -1)
		error_exit(true, "process_telnet: problem interpreting \"%s\"", bind_to.c_str());

        if (bind(listen_fd, (struct sockaddr *)&server_addr, sizeof server_addr) == -1)
                error_exit(true, "process_telnet: failed to bind to port %d", listen_port);

        if (listen(listen_fd, q_size) == -1)
                error_exit(true, "process_telnet: listen failed");

	// only 1 telnet user concurrently currently
	std::unique_lock<std::mutex> lck(clients->lock);

	client_t *client = new client_t();
	auto it_client = clients->clients.insert({ "_telnet_", client });

	lck.unlock();

	struct pollfd fds[] = { { listen_fd, POLLIN, 0 }, { -1, POLLIN, 0 } };

	bool  telnet_recv = false;
	int   telnet_left = 0;
	bool  telnet_sb   = false;

	while(!stop) {
		int rc = poll(fds, client_fd != -1 ? 2 : 1, 100);

		// transmit any queued traffic
		std::unique_lock<std::mutex> lck(it_client.first->second->lock);
		while(!it_client.first->second->queue.empty()) {
			std::string data = it_client.first->second->queue.at(0);

			it_client.first->second->queue.erase(it_client.first->second->queue.begin() + 0);

			if (client_fd != -1) {
				if (WRITE(client_fd, reinterpret_cast<const uint8_t *>(data.c_str()), data.size()) != ssize_t(data.size())) {
					close(client_fd);

					client_fd = -1;
				}
			}
		}

		lck.unlock();

		if (rc == 0)
			continue;

		if (rc == -1)
			error_exit(true, "process_telnet: poll failed");

		if (fds[0].revents) {
			if (client_fd != -1) {
				close(client_fd);

				telnet_recv = false;
				telnet_left = 0;
				telnet_sb   = false;
			}

			client_fd = accept(listen_fd, nullptr, nullptr);
			fds[1].fd = client_fd;

			std::string setup   = setup_telnet_session();

			std::string data    = generate_initial_screen(t);

			std::string initial = setup + data;

			if (WRITE(client_fd, reinterpret_cast<const uint8_t *>(initial.c_str()), initial.size()) != ssize_t(initial.size())) {
				close(client_fd);
				client_fd = -1;
			}
		}

		if (client_fd != -1 && fds[1].revents) {
			char buffer[4096];
			int  rc = read(client_fd, buffer, sizeof buffer);

			if (rc == -1 || rc == 0) {
				close(client_fd);
				client_fd = -1;
			}
			else {
				for(int i=0; i<rc; i++) {
					uint8_t c = buffer[i];

					if (telnet_left > 0 && telnet_sb == false) {
						if (c == 250)  // SB
							telnet_sb = true;

						telnet_left--;

						continue;
					}

					if (telnet_sb) {
						if (c == 240)  // SE
							telnet_sb = false;

						continue;
					}

					if (c == 0xff) {
						telnet_recv = true;
						telnet_left = 2;
					}
					else if (WRITE(program_fd, &c, 1) != 1) {
						assert(telnet_recv == false);
						assert(telnet_left == 0);

						if (client_fd != -1)  {
							close(client_fd);

							client_fd = 1;
						}

						stop = true;  // program went away
					}
				}
			}
		}
	}
}

void read_and_distribute_program(const int program_fd, terminal *const t, clients_t *const clients, const bool local_output)
{
	set_thread_name("program-i/o");

	printf("\033[8;%d;%dt\033[2J", t->get_height(), t->get_width());

	struct pollfd fds[] = { { program_fd, POLLIN, 0 } };

	while(!stop) {
		int prc = poll(fds, 1, 500);
		if (prc == -1)
			error_exit(true, "read_and_distribute_program: poll failed");

		if (prc == 0)
			continue;

		if (fds[0].revents) {
			char buffer[4096];

			int rrc = read(program_fd, buffer, sizeof buffer);
			if (rrc == -1 || rrc == 0) {
				dolog(ll_warning, "read_and_distribute_program: problem receiving from program %s", rrc ? strerror(errno) : "");
				break;
			}

			if (rrc > 0) {
				t->process_input(buffer, rrc);

				std::string data(buffer, rrc);

				if (local_output) {
					printf("%s", data.c_str());
					fflush(stdout);
				}

				std::lock_guard(clients->lock);

				for(auto & client : clients->clients) {
					std::lock_guard(client.second->lock);

					client.second->queue.push_back(data);
				}
			}
		}
	}
}

typedef struct
{
	terminal *t;

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

std::pair<uint8_t *, size_t> get_png_frame(terminal *const t, uint64_t *const ts_after)
{
	uint8_t *out   = nullptr;
	int      out_w = 0;
	int      out_h = 0;
	t->render(ts_after, &out, &out_w, &out_h);

	char *data_out = nullptr;
	size_t data_out_len = 0;

	FILE *fh = open_memstream(&data_out, &data_out_len);

	write_PNG_file(fh, out_w, out_h, out);

	fclose(fh);

	free(out);

	return { reinterpret_cast<uint8_t *>(data_out), data_out_len };
}

ssize_t stream_producer(void *cls, uint64_t pos, char *buf, size_t max)
{
	http_parameters_t *p = reinterpret_cast<http_parameters_t *>(cls);

	if (p->buffer == nullptr) {
		auto png = get_png_frame(p->t, &p->buffer_ts);

		int header_len = asprintf(reinterpret_cast<char **>(&p->buffer), "--12345\r\nContent-Type: image/png\r\nContent-Length: %zu\r\n\r\n", png.second);

		uint8_t *temp = reinterpret_cast<uint8_t *>(realloc(p->buffer, header_len + png.second));
		if (!temp)
			return 0;

		p->buffer = temp;

		memcpy(&p->buffer[header_len], png.first, png.second);

		p->bytes_in_buffer = header_len + png.second;

		free(png.first);
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

	terminal *t = reinterpret_cast<terminal *>(cls);

	if (strcmp(url, "/") == 0) {
		uint64_t after_ts = 0;
		auto     png      = get_png_frame(t, &after_ts);

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

	if (strcmp(url, "/stream") == 0) {
		http_parameters_t *parameters = reinterpret_cast<http_parameters_t *>(calloc(1, sizeof(http_parameters_t)));
		if (!parameters)
			return MHD_NO;

		parameters->t = t;

		struct MHD_Response *response = MHD_create_response_from_callback(MHD_SIZE_UNKNOWN, t->get_width() * t->get_height() * 3 * 2, &stream_producer, parameters, reinterpret_cast<MHD_ContentReaderFreeCallback>(free_parameters));

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

int main(int argc, char *argv[])
{
	std::string cfg_file = "termcamng.yaml";

	if (argc == 2)
		cfg_file = argv[1];

	YAML::Node config = YAML::LoadFile(cfg_file);

	std::string font_file = yaml_get_string(config, "font-file", "MS-DOS 8x16 console bitmap font-file");
	font f(font_file);

	const int width               = yaml_get_int(config,    "width",        "terminal console width (e.g. 80)");
	const int height              = yaml_get_int(config,    "height",       "terminal console height (e.g. 25)");

	const std::string telnet_bind = yaml_get_string(config, "telnet-addr",  "network interface (IP address) to let the telnet port bind to");
	const int telnet_port         = yaml_get_int(config,    "telnet-port",  "telnet port to listen on (0 to disable)");

	const int http_port           = yaml_get_int(config,    "http-port",    "HTTP port to serve PNG rendering of terminal");

	const int ssh_port            = yaml_get_int(config,    "ssh-port",     "SSH port for controlling the program (0 to disable)");
	const std::string ssh_bind    = yaml_get_string(config, "ssh-addr",     "network interface (IP address) to let the SSH port bind to");
	const std::string ssh_keys    = yaml_get_string(config, "ssh-keys",     "directory where the SSH keys are stored");

	const bool local_output       = yaml_get_bool(config,   "local-output", "show program output locally as well");
	const bool do_fork            = yaml_get_bool(config,   "fork",         "fork into the background");

	signal(SIGINT, signal_handler);

	terminal t(&f, width, height, &stop);

	std::string command    = yaml_get_string(config,  "exec-command", "command to execute and render");
	std::string directory  = yaml_get_string(config,  "directory",    "path to chdir for");

	// configure logfile
	YAML::Node cfg_log     = yaml_get_yaml_node(config, "logging",    "configuration of logging output");
	std::string logfile    = yaml_get_string(cfg_log, "file",         "file to log to");

	log_level_t ll_file    = str_to_ll(yaml_get_string(cfg_log, "loglevel-files",  "log-level for log-file"));
	log_level_t ll_screen  = str_to_ll(yaml_get_string(cfg_log, "loglevel-screen", "log-level for screen output"));

	setlog(logfile.c_str(), ll_file, ll_screen);

	if (do_fork) {
		if (daemon(1, 1) == -1)
			error_exit(true, "main: failed to fork into the background");
	}

	// main functionality
	clients_t clients;

	auto proc             = exec_with_pipe(command, directory, width, height);
	int  program_fd       = std::get<1>(proc);

	std::thread read_program([&clients, &t, program_fd, local_output] { read_and_distribute_program(program_fd, &t, &clients, local_output); });

	std::thread telnet_thread_handle([&t, program_fd, width, height, telnet_bind, telnet_port, &clients] {
			set_thread_name("telnet");

			if (telnet_port != 0)
				process_telnet(&t, program_fd, width, height, telnet_bind, telnet_port, &clients);
				});

	std::thread ssh_thread_handle([&t, program_fd, ssh_keys, ssh_bind, ssh_port, &clients] {
			set_thread_name("ssh");

			if (ssh_port != 0)
				process_ssh(&t, ssh_keys, ssh_bind, ssh_port, program_fd, &clients);
				});

	struct MHD_Daemon *d = MHD_start_daemon(
			MHD_USE_SELECT_INTERNALLY | MHD_USE_DEBUG,
			http_port,
			nullptr, nullptr, &get_terminal_png_frame, reinterpret_cast<void *>(&t),
			MHD_OPTION_CONNECTION_TIMEOUT, (unsigned int) 120,
			MHD_OPTION_END);

	while(!stop)
		sleep(1);

	ssh_thread_handle.join();

	telnet_thread_handle.join();

	read_program.join();

	MHD_stop_daemon(d);

	return 0;
}
