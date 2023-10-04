#include <string>


int start_tcp_listen(const std::string & bind_to, const int listen_port);

std::string get_endpoint_name(int fd);
