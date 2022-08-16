#ifndef   _CONDOR_SSL_COMPAT_H
#define   _CONDOR_SSL_COMPAT_H

#if OPENSSL_VERSION_NUMBER < 0x10100000L || defined(LIBRESSL_VERSION_NUMBER)
#define condor_EVP_MD_CTX_new  EVP_MD_CTX_create
#define condor_EVP_MD_CTX_free EVP_MD_CTX_destroy
#else
#define condor_EVP_MD_CTX_new  EVP_MD_CTX_new
#define condor_EVP_MD_CTX_free EVP_MD_CTX_free
#endif

#endif /* _CONDOR_SSL_COMPAT_H */
