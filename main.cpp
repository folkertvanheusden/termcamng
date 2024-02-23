#include <assert.h>
#include <atomic>
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
#include <wolfssl/options.h>
#include <wolfssl/ssl.h>

#include "error.h"
#include "http.h"
#include "io.h"
#include "logging.h"
#include "net.h"
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

			out += myformat("\033[%d;%dm%c", c.fg_col_ansi, c.bg_col_ansi, c.c ? c.c : ' ');
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

void process_ssh(terminal *const t, const std::string & ssh_keys, const std::string & bind_addr, const int port, const int program_fd, const bool dumb_telnet, const bool ignore_keypresses, clients_t *const clients)
{
	// setup ssh server
	ssh_bind sshbind = ssh_bind_new();

	ssh_bind_options_set(sshbind, SSH_BIND_OPTIONS_RSAKEY, myformat("%s/ssh_host_rsa_key", ssh_keys.c_str()).c_str());

	int server_fd = start_tcp_listen(bind_addr, port);

	struct pollfd fds[] = { { server_fd, POLLIN, 0 } };

	for(;!stop;) {
		bool err = false;

		int prc = poll(fds, 1, 500);
		if (prc == 0)
			continue;

		if (prc == -1)
			error_exit(true, "process_ssh: poll failed");

		if (fds[0].revents) {
			// wait for client
			int client_fd = accept(server_fd, nullptr, nullptr);
			if (client_fd == -1) {
				dolog(ll_warning, "process_ssh: error accepting a connection");
				continue;
			}

			dolog(ll_info, "process_ssh: connected with %s", get_endpoint_name(client_fd).c_str());

			ssh_session session = ssh_new();

			int r = ssh_bind_accept_fd(sshbind, session, client_fd);
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

					auto auth_rc = authenticate_against_pam(requested_user, ssh_message_auth_password(message));

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

			std::thread client_thread([session, username, channel, clients, t, program_fd, dumb_telnet, ignore_keypresses] {
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

						if (i > 0 && ignore_keypresses == false) {
							if (WRITE(program_fd, reinterpret_cast<const uint8_t *>(buffer), i) != i)
								break;
						}

						// transmit any queued traffic
						bool dumb_refresh = false;

						std::unique_lock<std::mutex> lck(it_client.first->second->lock);
						while(!it_client.first->second->queue.empty()) {
							std::string data = it_client.first->second->queue.at(0);

							it_client.first->second->queue.erase(it_client.first->second->queue.begin() + 0);

							if (!dumb_telnet)
								ssh_channel_write(channel, data.c_str(), data.size());

							dumb_refresh = true;
						}

						lck.unlock();

						if (dumb_refresh && dumb_telnet) {
							std::string data = generate_initial_screen(t);

							ssh_channel_write(channel, data.c_str(), data.size());
						}
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
	}

	ssh_finalize();

	printf("process_ssh: thread terminating\n");
}

void process_telnet(terminal *const t, const int program_fd, const int width, const int height, const std::string & bind_to, const int listen_port, const bool dumb_telnet, const bool ignore_keypresses, clients_t *const clients, const bool telnet_workarounds)
{
	int client_fd = -1;

	// setup listening socket for viewers
	int listen_fd = start_tcp_listen(bind_to, listen_port);

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
		bool dumb_refresh = false;

		std::unique_lock<std::mutex> lck(it_client.first->second->lock);
		while(!it_client.first->second->queue.empty()) {
			std::string data = it_client.first->second->queue.at(0);

			it_client.first->second->queue.erase(it_client.first->second->queue.begin() + 0);

			dumb_refresh = true;

			if (client_fd != -1 && dumb_telnet == false) {
				if (WRITE(client_fd, reinterpret_cast<const uint8_t *>(data.c_str()), data.size()) != ssize_t(data.size())) {
					close(client_fd);

					client_fd = -1;
				}
			}
		}

		lck.unlock();

		if (client_fd != -1 && dumb_refresh && dumb_telnet) {
			std::string data = generate_initial_screen(t);

			if (WRITE(client_fd, reinterpret_cast<const uint8_t *>(data.c_str()), data.size()) != ssize_t(data.size())) {
				close(client_fd);
				client_fd = -1;
			}
		}

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

			dolog(ll_info, "process_telnet: connected with %s", get_endpoint_name(client_fd).c_str());

			std::string setup   = setup_telnet_session();

			std::string data    = generate_initial_screen(t);

			std::string initial = setup + data;

			if (size_t(WRITE(client_fd, reinterpret_cast<const uint8_t *>(initial.c_str()), initial.size())) != initial.size()) {
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
					else if (ignore_keypresses == false)  {
						if (telnet_workarounds && c == 0)
							continue;

						if (WRITE(program_fd, &c, 1) != 1) {
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
}

void read_and_distribute_program(const int program_fd, terminal *const t, clients_t *const clients, const bool local_output)
{
	set_thread_name("program-i/o");

	if (local_output)
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
				auto send_back = t->process_input(buffer, rrc);

				if (send_back.has_value()) {
					if (WRITE(program_fd, reinterpret_cast<const uint8_t *>(send_back.value().c_str()), send_back.value().size()) != ssize_t(send_back.value().size())) {
						dolog(ll_warning, "read_and_distribute_program: problem responding to program %s", rrc ? strerror(errno) : "");
						break;
					}
				}

				std::string data(buffer, rrc);

				if (local_output) {
					printf("%s", data.c_str());
					fflush(stdout);
				}

#ifdef LOG_TRAFFIC
				{
					FILE *fh = fopen("traffic.log", "a+");

					if (fh) {
						fprintf(fh, "%s", data.c_str());

						fclose(fh);
					}
				}
#endif

				std::lock_guard(clients->lock);

				for(auto & client : clients->clients) {
					std::lock_guard(client.second->lock);

					client.second->queue.push_back(data);
				}
			}
		}
	}
}

void wolfssl_logging_cb(const int level, const char *const msg)
{
	dolog(ll_debug, "%d] %s", level, msg);
}

int main(int argc, char *argv[])
{
	try {
		std::string cfg_file          = argc == 2 ? argv[1] : "termcamng.yaml";

		if (argc == 2)
			cfg_file = argv[1];

#ifndef NDEBUG
		wolfSSL_SetLoggingCb(wolfssl_logging_cb);
		wolfSSL_Debugging_ON();
#endif

		wolfSSL_Init();

		YAML::Node config             = YAML::LoadFile(cfg_file);

		const int font_height         = yaml_get_int(config,    "font-height",  "font height (in pixels)");

		YAML::Node font_map           = yaml_get_yaml_node(config, "font-files", "TTF font file");

		std::vector<std::string> font_files;

		for(YAML::const_iterator it = font_map.begin(); it != font_map.end(); it++) {
			const std::string file = it->as<std::string>();

			font_files.push_back(file);
		}

		font f(font_files, font_height);

		const int width               = yaml_get_int(config,    "width",        "terminal console width (e.g. 80)");
		const int height              = yaml_get_int(config,    "height",       "terminal console height (e.g. 25)");
		const int compression_level   = yaml_get_int(config,    "compression-level", "value between 0 (no compression) and 100 (max.)");

		const std::string telnet_bind = yaml_get_string(config, "telnet-addr",  "network interface (IP address) to let the telnet port bind to");
		const int telnet_port         = yaml_get_int(config,    "telnet-port",  "telnet port to listen on (0 to disable)");

		const std::string http_bind   = yaml_get_string(config, "http-addr",    "network interface (IP address) to let the http port bind to");
		const int http_port           = yaml_get_int(config,    "http-port",    "HTTP port to serve PNG/JPEG rendering of terminal");

		std::string https_bind;
		std::string https_key;
		std::string https_cert;
		int https_port                = yaml_get_int(config,    "https-port",    "HTTPS port to serve PNG/JPEG rendering of terminal");
		if (https_port != 0) {
			https_bind = yaml_get_string(config, "http-addr",  "network interface (IP address) to let the http port bind to");

			https_key  = yaml_get_string(config, "https-key",         "https private key");
			https_cert = yaml_get_string(config, "https-certificate", "https certificate");
		}

		const int minimum_fps         = yaml_get_int(config,    "minimum-fps",  "minimum number of frame per second; set to 0 to not control this");

		const int ssh_port            = yaml_get_int(config,    "ssh-port",     "SSH port for controlling the program (0 to disable)");
		const std::string ssh_bind    = yaml_get_string(config, "ssh-addr",     "network interface (IP address) to let the SSH port bind to");
		const std::string ssh_keys    = yaml_get_string(config, "ssh-keys",     "directory where the SSH keys are stored");

		const bool local_output       = yaml_get_bool(config,   "local-output", "show program output locally as well");
		const bool do_fork            = yaml_get_bool(config,   "fork",         "fork into the background");

		const bool dumb_telnet        = yaml_get_bool(config,   "dumb-telnet",  "dumb, slow refresh for telnet sessions; this may increase compatibility with differing resolutions");

		const bool telnet_workarounds = yaml_get_bool(config,   "telnet-workarounds", "e.g. filter spurious 0x00");

		const bool ignore_keypresses  = yaml_get_bool(config,   "ignore-keypresses", "should key-presses from telnet/ssh clients be ignored");

		signal(SIGINT,  signal_handler);
		signal(SIGPIPE, SIG_IGN);

		terminal t(&f, width, height, &stop);

		const std::string command    = yaml_get_string(config,  "exec-command", "command to execute and render");
		const std::string directory  = yaml_get_string(config,  "directory",    "path to chdir for");
		const int restart_interval   = yaml_get_int(config,     "restart-interval", "when the command terminates, how long to wait (in seconds) to restart it, set to -1 to disable restarting");
		const bool stderr_to_stdout  = yaml_get_bool(config,    "stderr-to-stdout", "when set to true, stderr is visible. when set to false, stderr is send to /dev/null");

		// configure logfile
		YAML::Node cfg_log     = yaml_get_yaml_node(config, "logging",    "configuration of logging output");
		std::string logfile    = yaml_get_string(cfg_log,   "file",       "file to log to");

		log_level_t ll_file    = str_to_ll(yaml_get_string(cfg_log, "loglevel-files",  "log-level for log-file"));
		log_level_t ll_screen  = str_to_ll(yaml_get_string(cfg_log, "loglevel-screen", "log-level for screen output"));

		setlog(logfile.c_str(), ll_file, ll_screen);

		if (do_fork) {
			if (daemon(1, 1) == -1)
				error_exit(true, "main: failed to fork into the background");
		}

		// main functionality
		clients_t clients;

		auto proc             = exec_with_pipe(command, directory, width, height, restart_interval, stderr_to_stdout);
		int  program_fd       = std::get<1>(proc);

		std::thread read_program([&clients, &t, program_fd, local_output] { read_and_distribute_program(program_fd, &t, &clients, local_output); });

		std::thread telnet_thread_handle([&t, program_fd, width, height, telnet_bind, telnet_port, dumb_telnet, ignore_keypresses, &clients, telnet_workarounds] {
				set_thread_name("telnet");

				if (telnet_port != 0)
					process_telnet(&t, program_fd, width, height, telnet_bind, telnet_port, dumb_telnet, ignore_keypresses, &clients, telnet_workarounds);
				});

		std::thread ssh_thread_handle([&t, program_fd, ssh_keys, ssh_bind, ssh_port, dumb_telnet, ignore_keypresses, &clients] {
				set_thread_name("ssh");

				if (ssh_port != 0)
					process_ssh(&t, ssh_keys, ssh_bind, ssh_port, program_fd, dumb_telnet, ignore_keypresses, &clients);
				});

		http_server_parameters_t server_parameters { 0 };
		server_parameters.t                 = &t;
		server_parameters.compression_level = compression_level;
		server_parameters.max_wait          = minimum_fps > 0 ? 1000 / minimum_fps : 0;

		httpd *s_h = { nullptr };
		httpd *h   = start_http_server(http_bind, http_port, &server_parameters, { });

		if (https_port != 0) {
			if (https_cert.empty())
				error_exit(true, "Need to select either both certificate and key-file");

			auto https_cert_contents = load_text_file(https_cert);

			if (https_cert_contents.has_value() == false)
				error_exit(true, "Could not load https cert file");

			if (https_key.empty())
				error_exit(true, "Need to select either both certificate and key-file");

			auto https_key_contents = load_text_file(https_key);

			if (https_key_contents.has_value() == false)
				error_exit(true, "Could not load https key file");

			std::optional<std::pair<std::string, std::string> > tls_key_certificate = { { https_key_contents.value(), https_cert_contents.value() } };

			s_h = start_http_server(https_bind, https_port, &server_parameters, tls_key_certificate);
		}

		while(!stop)
			sleep(1);

		dolog(ll_info, "Stopping...");

		ssh_thread_handle.join();

		telnet_thread_handle.join();

		read_program.join();

		if (s_h)
			stop_http_server(s_h);

		stop_http_server(h);

		wolfSSL_Cleanup();
	}
	catch(const std::string & exception) {
		error_exit(true, "Program failure: %s", exception.c_str());
	}

	return 0;
}
