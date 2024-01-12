#include <string>
#include <wolfssl/options.h>
#include <wolfssl/ssl.h>

#include "net-io.h"


class net_io_wolfssl : public net_io
{
private:
	WOLFSSL_CTX *ctx { nullptr };
	WOLFSSL     *ssl { nullptr };

public:
	net_io_wolfssl(const int fd, const std::string & private_key, const std::string & certificate);
	virtual ~net_io_wolfssl();

	bool send(const uint8_t *const out, const size_t n) override;
	int  read(uint8_t *const out, const size_t n) override;
};
