#include <microhttpd.h>

#include "terminal.h"


struct MHD_Daemon * start_http_server(const int http_port, terminal *const t);
void                stop_http_server (struct MHD_Daemon *const d);
