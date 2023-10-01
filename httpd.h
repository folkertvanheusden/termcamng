#include <atomic>
#include <functional>
#include <map>
#include <optional>
#include <string>
#include <thread>

#include "net-io.h"


class httpd
{
private:
	const std::map<std::string, std::function<void (const std::string, net_io *const io, const void *, std::atomic_bool & stop_flag)> > url_map;
	const void *const parameters { nullptr };

	int               server_fd  { -1      };
	net_io           *io         { nullptr };
	std::thread      *th         { nullptr };
	std::atomic_bool  stop_flag  { false   };

	void handle_request(const int fd);

public:
	httpd(const std::string & bind_interface, const int bind_port, const std::map<std::string, std::function<void (const std::string, net_io *const io, const void *, std::atomic_bool & stop_flag)> > & url_map, const void *const parameters);
	virtual ~httpd();

	void operator()();
};
