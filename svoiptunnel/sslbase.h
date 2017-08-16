#ifndef __SSLBASE_H_
#define __SSLBASE_H_

#include <openssl/ssl.h>
#include <openssl/bio.h>
#include <openssl/err.h>
#include <openssl/rand.h>

#define SSL_BUFFER_SIZE		1024

class sslbase
{
protected:
	enum EMode
	{
		eSSLClient,
		eSSLServer,
		eSSLAccept
	};

public:
	sslbase();
	~sslbase();

	//static members used in OpenSSL threading support
	static int THREAD_setup();
	static int THREAD_cleanup();
	static void locking_function(int mode, int n, const char *file, int line);
	static unsigned long id_function();
	static int generate_cookie(SSL *ssl, unsigned char *cookie, unsigned int *cookie_len);
	static int verify_cookie(SSL *ssl, const unsigned char *cookie, unsigned int cookie_len);
	static int dtls_verify_callback (int ok, X509_STORE_CTX *ctx);

	static bool ssl_is_fatal_error(int ssl_error);
	static int ssl_get_error(SSL *ssl, int result);

protected:
#if WIN32
	static HANDLE* _mutex_buf;
#else
	static pthread_mutex_t* _mutex_buf;
#endif
};

#endif //__SSLBASE_H_
