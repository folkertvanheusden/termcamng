#include "net-io.h"


class net_io_fd : public net_io
{
private:
	const int fd { -1 };

public:
	net_io_fd(const int fd);
	virtual ~net_io_fd();

	bool send(const uint8_t *const out, const size_t n) override;
	std::optional<uint8_t> read() override;
};
