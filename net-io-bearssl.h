#include <string>
#include <bearssl/bearssl.h>

#include "BearSSLHelpers.h"
#include "net-io.h"


class net_io_bearssl : public net_io
{
private:
	int                   fd  { -1 };

	br_ssl_server_context sc  { { 0 } };
	unsigned char         iobuf[BR_SSL_BUFSIZE_BIDI] { 0 };
	br_sslio_context      ioc { 0 };

	BearSSL::X509List    *c          { nullptr };
	BearSSL::PrivateKey  *pk         { nullptr };

	const br_x509_certificate *br_c  { nullptr };
	size_t                br_c_count { 0       };

public:
	net_io_bearssl(const int fd, const std::string & private_key, const std::string & certificate);
	virtual ~net_io_bearssl();

	bool send(const uint8_t *const out, const size_t n) override;
	bool read(uint8_t *const out, const size_t n) override;
};
