#include <stdio.h>
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

int net_io_fd::read(uint8_t *const out, const size_t n)
{
	return ::read(fd, out, n);
}
