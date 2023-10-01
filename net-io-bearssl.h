#include "net-io.h"


class net_io_bearssl : public net_io
{
public:
	net_io_bearssl();
	virtual ~net_io_bearssl();

	bool send(const uint8_t *const out, const size_t n) override;
	bool read(uint8_t *const out, const size_t n) override;
};
