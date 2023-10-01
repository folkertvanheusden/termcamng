#pragma once

#include <cstdint>
#include <optional>


class net_io
{
public:
	net_io();
	virtual ~net_io();

	virtual bool send(const uint8_t *const out, const size_t n) = 0;
	virtual bool read(uint8_t *const out, const size_t n) = 0;
};
