#include <unistd.h>

#include "io.h"
#include "net-io-fd.h"


net_io_fd::net_io_fd(const int fd) : fd(fd)
{
}

net_io_fd::~net_io_fd()
{
}

bool net_io_fd::send(const uint8_t *const out, const size_t n)
{
	return WRITE(fd, out, n) == ssize_t(n);
}

std::optional<uint8_t> net_io_fd::read()
{
	uint8_t b = 0;
	int rc = ::read(fd, &b, 1);

	if (rc == 1)
		return b;

	return { };
}
