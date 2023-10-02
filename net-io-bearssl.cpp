#include <unistd.h>

#include "error.h"
#include "io.h"
#include "logging.h"
#include "net-io-bearssl.h"


int sock_read(void *ctx, unsigned char *buf, size_t len)
{
	int fd = *reinterpret_cast<int *>(ctx);

	return read(fd, buf, len);
}

int sock_write(void *ctx, const unsigned char *buf, size_t len)
{
	int fd = *reinterpret_cast<int *>(ctx);

	return write(fd, buf, len);
}

net_io_bearssl::net_io_bearssl(const int fd, const std::string & private_key, const std::string & certificate) : fd(fd)
{
	c          = new BearSSL::X509List(certificate.c_str());
	br_c       = c->getX509Certs();
	br_c_count = c->getCount();

	pk         = new BearSSL::PrivateKey(private_key.c_str());

	if (pk->isRSA()) {
		dolog(ll_debug, "net_io_bearssl: private key is an RSA key");

		const br_rsa_private_key *br_pk = pk->getRSA();

		br_ssl_server_init_full_rsa(&sc, br_c, br_c_count, br_pk);
	}
	else if (pk->isEC()) {
		dolog(ll_debug, "net_io_bearssl: private key is an EC key");

		const br_ec_private_key *br_pk = pk->getEC();

		br_ssl_server_init_full_ec(&sc, br_c, br_c_count, BR_KEYTYPE_EC, br_pk);
	}
	else {
		error_exit(false, "net_io_bearssl: private key is not RSA or EC");
	}

	br_ssl_engine_set_buffer(&sc.eng, iobuf, sizeof iobuf, 1);

	if (br_ssl_server_reset(&sc) == 0)
		dolog(ll_error, "net_io_bearssl: br_ssl_server_reset failed");

	br_sslio_init(&ioc, &sc.eng, sock_read, reinterpret_cast<void *>(&this->fd), sock_write, reinterpret_cast<void *>(&this->fd));
}

net_io_bearssl::~net_io_bearssl()
{
	br_sslio_close(&ioc);

	delete pk;

	delete c;
}

bool net_io_bearssl::send(const uint8_t *const out, const size_t n)
{
	int rc = br_sslio_write_all(&ioc, out, n);

	if (rc == 0) {
		br_sslio_flush(&ioc);

		return true;
	}

	dolog(ll_debug, "net_io_bearssl::send: failed transmitting %zu bytes: %d", n, rc);

	return false;
}

int net_io_bearssl::read(uint8_t *const out, const size_t n)
{
	return br_sslio_read(&ioc, out, n);
}
