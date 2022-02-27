#include <microhttpd.h>

#include "terminal.h"


typedef struct {
	terminal *t;
	int       compression_level;
} http_server_parameters_t;

struct MHD_Daemon * start_http_server(const int http_port, http_server_parameters_t *const hsp);
void                stop_http_server (struct MHD_Daemon *const d);
