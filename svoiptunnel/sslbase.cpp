#include "common.h"
#include "sslbase.h"
#include "log.h"

#ifdef WIN32
#define in_port_t u_short
#define ssize_t int
#endif//WIN32

#if WIN32
HANDLE* sslbase::_mutex_buf = NULL;
#else
pthread_mutex_t* sslbase::_mutex_buf = NULL;
#endif

//TODO: ...
#define COOKIE_SECRET_LENGTH 16
unsigned char cookie_secret[COOKIE_SECRET_LENGTH];
int cookie_initialized=0;

#define COUNT_OF(x) ((sizeof(x)/sizeof(0[x])) / ((size_t)(!(sizeof(x) % sizeof(0[x])))))

sslbase::sslbase()
{
}

sslbase::~sslbase()
{
}

bool sslbase::ssl_is_fatal_error(int ssl_error)
{
	switch(ssl_error)
	{
		case SSL_ERROR_NONE:
		case SSL_ERROR_WANT_READ:
		case SSL_ERROR_WANT_WRITE:
		case SSL_ERROR_WANT_CONNECT:
		case SSL_ERROR_WANT_ACCEPT:
			return false;
	}
	return true;
}

int sslbase::ssl_get_error(SSL *ssl, int result)
{
	int error = SSL_get_error(ssl, result);
	if(SSL_ERROR_NONE != error)
	{
		char message[512] = {0};
		int error_log = error;
		while(SSL_ERROR_NONE != error_log)
		{
			ERR_error_string_n(error_log, message, COUNT_OF(message));
			if(ssl_is_fatal_error(error_log))
			{
				// print error message to console or logs
				log_error() << "SSL error: " << message << endl;
			}
			error_log = ERR_get_error();
		}
	}
	return error;
}

int sslbase::THREAD_setup()
{
	int i;

#ifdef WIN32
	_mutex_buf = (HANDLE*) malloc(CRYPTO_num_locks() * sizeof(HANDLE));
#else
	_mutex_buf = (pthread_mutex_t*) malloc(CRYPTO_num_locks() * sizeof(pthread_mutex_t));
#endif
	if (!_mutex_buf)
		return 0;
	for (i = 0; i < CRYPTO_num_locks(); i++)
#ifdef WIN32
		_mutex_buf[i] = CreateMutex(NULL, FALSE, NULL);
#else
		pthread_mutex_init(&_mutex_buf[i], NULL);
#endif
	CRYPTO_set_id_callback(id_function);
	CRYPTO_set_locking_callback(locking_function);

	return 1;
}

int sslbase::THREAD_cleanup()
{
	int i;

	if (!_mutex_buf)
		return 0;

	CRYPTO_set_id_callback(NULL);
	CRYPTO_set_locking_callback(NULL);
	for (i = 0; i < CRYPTO_num_locks(); i++)
#ifdef WIN32
	CloseHandle(_mutex_buf[i]);
#else
	pthread_mutex_destroy(&_mutex_buf[i]);
#endif
	free(_mutex_buf);
	_mutex_buf = NULL;
	return 1;
}

void sslbase::locking_function(int mode, int n, const char *file, int line)
{
	if (mode & CRYPTO_LOCK)
#ifdef WIN32
		WaitForSingleObject(_mutex_buf[n], INFINITE);
	else
		ReleaseMutex(_mutex_buf[n]);
#else
		pthread_mutex_lock(&_mutex_buf[n]);
	else
		pthread_mutex_unlock(&_mutex_buf[n]);
#endif
}

unsigned long sslbase::id_function()
{
#ifdef WIN32
	return (unsigned long) GetCurrentThreadId();
#else
	return (unsigned long) pthread_self();
#endif
}

int sslbase::generate_cookie( SSL *ssl, unsigned char *cookie, unsigned int *cookie_len )
{
	unsigned char *buffer, result[EVP_MAX_MD_SIZE];
	unsigned int length = 0, resultlength;
	union {
		struct sockaddr_storage ss;
		struct sockaddr_in6 s6;
		struct sockaddr_in s4;
	} peer;

	/* Initialize a random secret */
	if (!cookie_initialized)
	{
		if (!RAND_bytes(cookie_secret, COOKIE_SECRET_LENGTH))
		{
			log_error() << "error setting random cookie secret" << endl;
			return 0;
		}
		cookie_initialized = 1;
	}

	/* Read peer information */
	(void) BIO_dgram_get_peer(SSL_get_rbio(ssl), &peer);

	/* Create buffer with peer's address and port */
	length = 0;
	switch (peer.ss.ss_family) {
		case AF_INET:
			length += sizeof(struct in_addr);
			break;
		case AF_INET6:
			length += sizeof(struct in6_addr);
			break;
		default:
			OPENSSL_assert(0);
			break;
	}
	length += sizeof(in_port_t);
	buffer = (unsigned char*) OPENSSL_malloc(length);

	if (buffer == NULL)
	{
		log_error() << "out of memory" << endl;
		return 0;
	}

	switch (peer.ss.ss_family) {
		case AF_INET:
			memcpy(buffer,
					&peer.s4.sin_port,
					sizeof(in_port_t));
			memcpy(buffer + sizeof(peer.s4.sin_port),
					&peer.s4.sin_addr,
					sizeof(struct in_addr));
			break;
		case AF_INET6:
			memcpy(buffer,
					&peer.s6.sin6_port,
					sizeof(in_port_t));
			memcpy(buffer + sizeof(in_port_t),
					&peer.s6.sin6_addr,
					sizeof(struct in6_addr));
			break;
		default:
			OPENSSL_assert(0);
			break;
	}

	/* Calculate HMAC of buffer using the secret */
	HMAC(EVP_sha1(), (const void*) cookie_secret, COOKIE_SECRET_LENGTH,
	     (const unsigned char*) buffer, length, result, &resultlength);
	OPENSSL_free(buffer);

	memcpy(cookie, result, resultlength);
	*cookie_len = resultlength;

	return 1;
}

int sslbase::verify_cookie( SSL *ssl, const unsigned char *cookie, unsigned int cookie_len )
{
	unsigned char *buffer, result[EVP_MAX_MD_SIZE];
	unsigned int length = 0, resultlength;
	union {
		struct sockaddr_storage ss;
		struct sockaddr_in6 s6;
		struct sockaddr_in s4;
	} peer;

	/* If secret isn't initialized yet, the cookie can't be valid */
	if (!cookie_initialized)
		return 0;

	/* Read peer information */
	(void) BIO_dgram_get_peer(SSL_get_rbio(ssl), &peer);

	/* Create buffer with peer's address and port */
	length = 0;
	switch (peer.ss.ss_family) {
		case AF_INET:
			length += sizeof(struct in_addr);
			break;
		case AF_INET6:
			length += sizeof(struct in6_addr);
			break;
		default:
			OPENSSL_assert(0);
			break;
	}
	length += sizeof(in_port_t);
	buffer = (unsigned char*) OPENSSL_malloc(length);

	if (buffer == NULL)
	{
		log_error() << "out of memory" << endl;
		return 0;
	}

	switch (peer.ss.ss_family) {
		case AF_INET:
			memcpy(buffer,
			       &peer.s4.sin_port,
			       sizeof(in_port_t));
			memcpy(buffer + sizeof(in_port_t),
			       &peer.s4.sin_addr,
			       sizeof(struct in_addr));
			break;
		case AF_INET6:
			memcpy(buffer,
			       &peer.s6.sin6_port,
			       sizeof(in_port_t));
			memcpy(buffer + sizeof(in_port_t),
			       &peer.s6.sin6_addr,
			       sizeof(struct in6_addr));
			break;
		default:
			OPENSSL_assert(0);
			break;
	}

	/* Calculate HMAC of buffer using the secret */
	HMAC(EVP_sha1(), (const void*) cookie_secret, COOKIE_SECRET_LENGTH,
	     (const unsigned char*) buffer, length, result, &resultlength);
	OPENSSL_free(buffer);

	if (cookie_len == resultlength && memcmp(result, cookie, resultlength) == 0)
		return 1;

	return 0;
}

int sslbase::dtls_verify_callback( int ok, X509_STORE_CTX *ctx )
{
	/* This function should ask the user
	 * if he trusts the received certificate.
	 * Here we always trust.
	 */
	return 1;
}
