#include <unistd.h>

#include "error.h"
#include "io.h"
#include "logging.h"
#include "net-io-wolfssl.h"


net_io_wolfssl::net_io_wolfssl(const int fd, const std::string & private_key, const std::string & certificate)
{
	ctx = wolfSSL_CTX_new(wolfTLSv1_2_server_method());

	if (wolfSSL_CTX_use_certificate_buffer(ctx, reinterpret_cast<const unsigned char *>(certificate.c_str()), certificate.size(), WOLFSSL_FILETYPE_PEM) != WOLFSSL_SUCCESS)
		error_exit(false, "Cannot access TLS certificate");

	if (wolfSSL_CTX_use_PrivateKey_buffer(ctx, reinterpret_cast<const unsigned char *>(private_key.c_str()), private_key.size(), WOLFSSL_FILETYPE_PEM) != WOLFSSL_SUCCESS)
		error_exit(false, "Cannot access private key");

	ssl = wolfSSL_new(ctx);
	if (!ssl)
		error_exit(false, "Cannot setup context for wolfssl");

	if (wolfSSL_set_fd(ssl, fd) != SSL_SUCCESS)
		dolog(ll_info, "Connecting fd to ssl context failed");
	else if (wolfSSL_accept(ssl) != WOLFSSL_SUCCESS) {
		int err_nr = wolfSSL_get_error(ssl, 0);
		char msg[80] { };
		wolfSSL_ERR_error_string(err_nr, msg);
		dolog(ll_info, "Accept on TLS socket failed: %s", msg);
	}
}

net_io_wolfssl::~net_io_wolfssl()
{
	if (ssl)
		wolfSSL_free(ssl);

	if (ctx)
		wolfSSL_CTX_free(ctx);
}

bool net_io_wolfssl::send(const uint8_t *const out, const size_t n)
{
	if (wolfSSL_write(ssl, out, n) == n)
		return true;

	dolog(ll_debug, "net_io_wolfssl::send: failed transmitting %zu bytes", n);

	return false;
}

int net_io_wolfssl::read(uint8_t *const out, const size_t n)
{
	return wolfSSL_read(ssl, out, n);
}
