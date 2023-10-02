#include "httpd.h"
#include "terminal.h"


typedef struct {
	terminal *t;
	int       compression_level;
	int       max_wait;
} http_server_parameters_t;

httpd * start_http_server(const std::string & bind_ip, const int http_port, http_server_parameters_t *const hsp, const std::optional<std::pair<std::string, std::string> > tls_key_certificate);
void    stop_http_server (httpd *const h);
