/* ssl.h                                
 *
 * Copyright (C) 2003 Sawtooth Consulting Ltd.
 *
 * This file is part of yaSSL.
 *
 * yaSSL is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * yaSSL is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA 02111-1307, USA
 */

/*  ssl.h defines openssl compatibility layer 
 *
 */

#ifndef ysSSL_openssl_h__
#define yaSSL_openssl_h__

#include <stdio.h>   /* ERR_print fp */
#include "rsa.h"

#if defined(__cplusplus) && !defined(YASSL_MYSQL_COMPATIBLE)
namespace yaSSL {
extern "C" {
#endif

#undef X509_NAME   /* wincrypt.h clash */

#if defined(__cplusplus) && !defined(YASSL_MYSQL_COMPATIBLE)
    class SSL;
    class SSL_SESSION;
    class SSL_METHOD;
    class SSL_CTX;
    class SSL_CIPHER;

    class RSA;

    class X509;
    class X509_NAME;
#else
    typedef struct SSL         SSL;          
    typedef struct SSL_SESION  SSL_SESSION;
    typedef struct SSL_METHOD  SSL_METHOD;
    typedef struct SSL_CTX     SSL_CTX;
    typedef struct SSL_CIPHER  SSL_CIPHER;

    typedef struct RSA RSA;

    typedef struct X509       X509;
    typedef struct X509_NAME  X509_NAME;
#endif


/* Big Number stuff, different file? */
typedef struct BIGNUM BIGNUM;

BIGNUM *BN_bin2bn(const unsigned char*, int, BIGNUM*);


/* Diffie-Hellman stuff, different file? */
/* mySQL deferences to set group parameters */
typedef struct DH {
    BIGNUM* p;
    BIGNUM* g;
} DH;

DH*  DH_new(void);
void DH_free(DH*);

/* RSA stuff */

void RSA_free(RSA*);
RSA* RSA_generate_key(int, unsigned long, void(*)(int, int, void*), void*);


/* X509 stuff, different file? */

typedef struct X509_STORE         X509_STORE;
typedef struct X509_LOOKUP        X509_LOOKUP;
typedef struct X509_OBJECT { char c; } X509_OBJECT;
typedef struct X509_CRL           X509_CRL;
typedef struct X509_REVOKED       X509_REVOKED;
typedef struct X509_LOOKUP_METHOD X509_LOOKUP_METHOD;


void X509_free(X509*);


/* bio stuff */
typedef struct BIO BIO;

/* ASN stuff */
typedef struct ASN1_TIME ASN1_TIME;



/* because mySQL dereferences to use error and current_cert, even after calling
 * get functions for local references */
typedef struct X509_STORE_CTX {
    int   error;
    int   error_depth;
    X509* current_cert;
} X509_STORE_CTX;



X509* X509_STORE_CTX_get_current_cert(X509_STORE_CTX*);
int   X509_STORE_CTX_get_error(X509_STORE_CTX*);
int   X509_STORE_CTX_get_error_depth(X509_STORE_CTX*);

char*       X509_NAME_oneline(X509_NAME*, char*, int);
X509_NAME*  X509_get_issuer_name(X509*);
X509_NAME*  X509_get_subject_name(X509*);
const char* X509_verify_cert_error_string(long);

int                 X509_LOOKUP_add_dir(X509_LOOKUP*, const char*, long);
int                 X509_LOOKUP_load_file(X509_LOOKUP*, const char*, long);
X509_LOOKUP_METHOD* X509_LOOKUP_hash_dir(void);
X509_LOOKUP_METHOD* X509_LOOKUP_file(void);

X509_LOOKUP* X509_STORE_add_lookup(X509_STORE*, X509_LOOKUP_METHOD*);
X509_STORE*  X509_STORE_new(void);
int          X509_STORE_get_by_subject(X509_STORE_CTX*, int, X509_NAME*,
                                       X509_OBJECT*);




enum { /* X509 Constants */
    X509_V_OK                                 =  0,
    X509_V_ERR_CERT_CHAIN_TOO_LONG            =  1,
    X509_V_ERR_UNABLE_TO_GET_ISSUER_CERT      =  2,
    X509_V_ERR_CERT_NOT_YET_VALID             =  3,
    X509_V_ERR_ERROR_IN_CERT_NOT_BEFORE_FIELD =  4,
    X509_V_ERR_CERT_HAS_EXPIRED               =  5,
    X509_V_ERR_ERROR_IN_CERT_NOT_AFTER_FIELD  =  6,
    X509_FILETYPE_PEM                         =  7,
    X509_LU_X509                              =  8,
    X509_LU_CRL                               =  9,
    X509_V_ERR_CRL_SIGNATURE_FAILURE          = 10,
    X509_V_ERR_ERROR_IN_CRL_NEXT_UPDATE_FIELD = 11,
    X509_V_ERR_CRL_HAS_EXPIRED                = 12,
    X509_V_ERR_CERT_REVOKED                   = 13

};


/* Error stuff, could move to yassl_error */
unsigned long ERR_get_error_line_data(const char**, int*, const char**, int *);
void          ERR_print_errors_fp(FILE*);
char*         ERR_error_string(unsigned long,char*);
void          ERR_remove_state(unsigned long);
unsigned long ERR_get_error(void);
unsigned long ERR_peek_error(void);
int           ERR_GET_REASON(int);


enum {  /* ERR Constants */
    ERR_TXT_STRING = 1,
    EVP_R_BAD_DECRYPT = 2
};



SSL_CTX* SSL_CTX_new(SSL_METHOD*);
SSL* SSL_new(SSL_CTX*);
int  SSL_set_fd (SSL*, int);
int  SSL_connect(SSL*);
int  SSL_write(SSL*, const void*, int);
int  SSL_read(SSL*, void*, int);
int  SSL_accept(SSL*);
void SSL_CTX_free(SSL_CTX*);
void SSL_free(SSL*);
int  SSL_clear(SSL*);
int  SSL_shutdown(SSL*);

void SSL_set_connect_state(SSL*);
void SSL_set_accept_state(SSL*);
int  SSL_do_handshake(SSL*);

const char* SSL_get_cipher(SSL*);
const char* SSL_get_cipher_name(SSL*);	           /* uses SSL_get_cipher */
char*       SSL_get_shared_ciphers(SSL*, char*, int);
const char* SSL_get_cipher_list(SSL*, int);
const char* SSL_get_version(SSL*);
const char* SSLeay_version(int);

int  SSL_get_error(SSL*, int);
void SSL_load_error_strings(void);

int          SSL_set_session(SSL *ssl, SSL_SESSION *session);
SSL_SESSION* SSL_get_session(SSL* ssl);
long         SSL_SESSION_set_timeout(SSL_SESSION*, long);
X509*        SSL_get_peer_certificate(SSL*);
long         SSL_get_verify_result(SSL*);


typedef int (*VerifyCallback)(int, X509_STORE_CTX*);
typedef int (*pem_password_cb)(char*, int, int, void*);

void SSL_CTX_set_verify(SSL_CTX*, int, VerifyCallback verify_callback);
int  SSL_CTX_load_verify_locations(SSL_CTX*, const char*, const char*);
int  SSL_CTX_set_default_verify_paths(SSL_CTX*);
int  SSL_CTX_check_private_key(SSL_CTX*);
int  SSL_CTX_set_session_id_context(SSL_CTX*, const unsigned char*,
                                    unsigned int);

void SSL_CTX_set_tmp_rsa_callback(SSL_CTX*, RSA*(*)(SSL*, int, int));
long SSL_CTX_set_options(SSL_CTX*, long);
long SSL_CTX_set_session_cache_mode(SSL_CTX*, long);
long SSL_CTX_set_timeout(SSL_CTX*, long);
int  SSL_CTX_use_certificate_chain_file(SSL_CTX*, const char*);
void SSL_CTX_set_default_passwd_cb(SSL_CTX*, pem_password_cb);
int  SSL_CTX_use_RSAPrivateKey_file(SSL_CTX*, const char*, int);
void SSL_CTX_set_info_callback(SSL_CTX*, void (*)());

long SSL_CTX_sess_accept(SSL_CTX*);
long SSL_CTX_sess_connect(SSL_CTX*);
long SSL_CTX_sess_accept_good(SSL_CTX*);
long SSL_CTX_sess_connect_good(SSL_CTX*);
long SSL_CTX_sess_accept_renegotiate(SSL_CTX*);
long SSL_CTX_sess_connect_renegotiate(SSL_CTX*);
long SSL_CTX_sess_hits(SSL_CTX*);
long SSL_CTX_sess_cb_hits(SSL_CTX*);
long SSL_CTX_sess_cache_full(SSL_CTX*);
long SSL_CTX_sess_misses(SSL_CTX*);
long SSL_CTX_sess_timeouts(SSL_CTX*);
long SSL_CTX_sess_number(SSL_CTX*);
long SSL_CTX_sess_get_cache_size(SSL_CTX*);

int SSL_CTX_get_verify_mode(SSL_CTX*);
int SSL_get_verify_mode(SSL*);
int SSL_CTX_get_verify_depth(SSL_CTX*);
int SSL_get_verify_depth(SSL*);

long SSL_get_default_timeout(SSL*);
long SSL_CTX_get_session_cache_mode(SSL_CTX*);
int  SSL_session_reused(SSL*);

int  SSL_set_rfd(SSL*, int);
int  SSL_set_wfd(SSL*, int);
void SSL_set_shutdown(SSL*, int);

int SSL_want_read(SSL*);
int SSL_want_write(SSL*);

int SSL_pending(SSL*);


enum { /* ssl Constants */
    SSL_BAD_FILETYPE    = -5,
    SSL_BAD_FILE        = -4,
    SSL_NOT_IMPLEMENTED = -3,
    SSL_UNKNOWN         = -2,
    SSL_FATAL_ERROR     = -1,
    SSL_NORMAL_SHUTDOWN =  0,
    SSL_ERROR_NONE      =  0,   /* for most functions */
    SSL_FAILURE         =  0,   /* for some functions */
    SSL_SUCCESS	        =  1,

    SSL_FILETYPE_ASN1    = 10,
    SSL_FILETYPE_PEM     = 11,
    SSL_FILETYPE_DEFAULT = 10, /* ASN1 */

    SSL_VERIFY_NONE                 = 0,
    SSL_VERIFY_PEER                 = 1,
    SSL_VERIFY_FAIL_IF_NO_PEER_CERT = 2,
    SSL_VERIFY_CLIENT_ONCE          = 4,

    SSL_SESS_CACHE_OFF                = 30,
    SSL_SESS_CACHE_CLIENT             = 31,
    SSL_SESS_CACHE_SERVER             = 32,
    SSL_SESS_CACHE_BOTH               = 33,
    SSL_SESS_CACHE_NO_AUTO_CLEAR      = 34,
    SSL_SESS_CACHE_NO_INTERNAL_LOOKUP = 35,

    SSL_OP_MICROSOFT_SESS_ID_BUG            = 50,
    SSL_OP_NETSCAPE_CHALLENGE_BUG           = 51,
    SSL_OP_NETSCAPE_REUSE_CIPHER_CHANGE_BUG = 52,
    SSL_OP_SSLREF2_REUSE_CERT_TYPE_BUG      = 53,
    SSL_OP_MICROSOFT_BIG_SSLV3_BUFFER       = 54,
    SSL_OP_MSIE_SSLV2_RSA_PADDING           = 55,
    SSL_OP_SSLEAY_080_CLIENT_DH_BUG         = 56,
    SSL_OP_TLS_D5_BUG                       = 57,
    SSL_OP_TLS_BLOCK_PADDING_BUG            = 58,
    SSL_OP_TLS_ROLLBACK_BUG                 = 59,
    SSL_OP_DONT_INSERT_EMPTY_FRAGMENTS      = 60,
    SSL_OP_ALL                              = 61,
    SSL_OP_SINGLE_DH_USE                    = 62,
    SSL_OP_EPHEMERAL_RSA                    = 63,
    SSL_OP_NO_SSLv2                         = 64,
    SSL_OP_NO_SSLv3                         = 65,
    SSL_OP_NO_TLSv1                         = 66,
    SSL_OP_PKCS1_CHECK_1                    = 67,
    SSL_OP_PKCS1_CHECK_2                    = 68,
    SSL_OP_NETSCAPE_CA_DN_BUG               = 69,
    SSL_OP_NON_EXPORT_FIRST                 = 70,
    SSL_OP_NETSCAPE_DEMO_CIPHER_CHANGE_BUG  = 71,

    SSL_ERROR_WANT_READ        = 80,
    SSL_ERROR_WANT_WRITE       = 81,
    SSL_ERROR_SYSCALL          = 82,
    SSL_ERROR_WANT_X509_LOOKUP = 83,
    SSL_ERROR_ZERO_RETURN      = 84,
    SSL_ERROR_SSL              = 85,

    SSL_SENT_SHUTDOWN     = 90,
    SSL_RECEIVED_SHUTDOWN = 91,
    SSL_CB_LOOP           = 92,
    SSL_ST_CONNECT        = 93,
    SSL_ST_ACCEPT         = 94,
    SSL_CB_ALERT          = 95,
    SSL_CB_READ           = 96,
    SSL_CB_HANDSHAKE_DONE = 97

};


SSL_METHOD *SSLv3_method(void);
SSL_METHOD *SSLv3_server_method(void);
SSL_METHOD *SSLv3_client_method(void);
SSL_METHOD *TLSv1_server_method(void);  
SSL_METHOD *TLSv1_client_method(void);
SSL_METHOD *SSLv23_server_method(void);

int SSL_CTX_use_certificate_file(SSL_CTX*, const char*, int);
int SSL_CTX_use_PrivateKey_file(SSL_CTX*, const char*, int);
int SSL_CTX_set_cipher_list(SSL_CTX*, const char*);

long SSL_CTX_sess_set_cache_size(SSL_CTX*, long);
long SSL_CTX_set_tmp_dh(SSL_CTX*, DH*);

void OpenSSL_add_all_algorithms(void);
void SSLeay_add_ssl_algorithms(void);


SSL_CIPHER* SSL_get_current_cipher(SSL*);
char*       SSL_CIPHER_description(SSL_CIPHER*, char*, int);


char* SSL_alert_type_string_long(int);
char* SSL_alert_desc_string_long(int);
char* SSL_state_string_long(SSL*);


/* EVP stuff, des and md5, different file? */
typedef struct Digest Digest;
typedef Digest EVP_MD;

typedef struct BulkCipher BulkCipher;
typedef BulkCipher EVP_CIPHER;

typedef struct EVP_PKEY EVP_PKEY;

typedef unsigned char DES_cblock[8];
typedef const  DES_cblock const_DES_cblock;
typedef DES_cblock DES_key_schedule;
                                                          
                                                             
const EVP_MD*     EVP_md5(void);
const EVP_CIPHER* EVP_des_ede3_cbc(void);

typedef unsigned char opaque;

int EVP_BytesToKey(const EVP_CIPHER*, const EVP_MD*, const opaque*,
                   const opaque*, int, int, opaque*, opaque*);

void DES_set_key_unchecked(const_DES_cblock*, DES_key_schedule*);
void DES_ede3_cbc_encrypt(const opaque*, opaque*, long, DES_key_schedule*,
                        DES_key_schedule*, DES_key_schedule*, DES_cblock*, int);


/* RAND stuff */
void        RAND_screen(void);
const char* RAND_file_name(char*, size_t);
int         RAND_write_file(const char*);
int         RAND_load_file(const char*, long);


#define SSL_DEFAULT_CIPHER_LIST ""   /* default all */




#if defined(__cplusplus) && !defined(YASSL_MYSQL_COMPATIBLE)
}      /* namespace  */
}      /* extern "C" */
#endif


#endif /* yaSSL_openssl_h__ */
