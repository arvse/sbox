/* ------------------------------------------------------------------
 * SBox - Simple Data Achive Utility
 * ------------------------------------------------------------------ */

#include "sbox.h"
#include <mbedtls/aes.h>
#include <mbedtls/sha256.h>
#include <mbedtls/pkcs5.h>

#ifdef ENABLE_ENCRYPTION

#define AES256_KEYLEN 32
#define AES256_KEYLEN_BITS (AES256_KEYLEN*8)
#define AES256_BLOCKLEN 16
#define SHA256_BLOCKLEN 32
#define DERIVE_N_ROUNDS 50000

struct io_aes_context_t
{
    int fd;
    int eof;
    size_t u_len;
    size_t unaligned;
    mbedtls_aes_context aes;
    mbedtls_md_context_t md_ctx;
    unsigned char salt[AES256_KEYLEN];
    unsigned char iv[AES256_BLOCKLEN];
    unsigned char unconsumed[CHUNK];
    unsigned char hmac[SHA256_BLOCKLEN];
};

static int pbkdf2_sha256_derive_key ( const char *password, const unsigned char *salt,
    size_t salt_len, unsigned char *key, size_t key_size )
{
    mbedtls_md_context_t sha256_ctx;
    const mbedtls_md_info_t *sha256_info;

    mbedtls_md_init ( &sha256_ctx );

    if ( !( sha256_info = mbedtls_md_info_from_type ( MBEDTLS_MD_SHA256 ) ) )
    {
        mbedtls_md_free ( &sha256_ctx );
        return -1;
    }

    if ( mbedtls_md_setup ( &sha256_ctx, sha256_info, 1 ) != 0 )
    {
        mbedtls_md_free ( &sha256_ctx );
        return -1;
    }

    if ( mbedtls_pkcs5_pbkdf2_hmac ( &sha256_ctx, ( const unsigned char * ) password,
            strlen ( password ), salt, salt_len, DERIVE_N_ROUNDS, key_size, key ) != 0 )
    {
        memset ( key, '\0', key_size );
        mbedtls_md_free ( &sha256_ctx );
        return -1;
    }

    mbedtls_md_free ( &sha256_ctx );
    return 0;
}

/*
 * Read data chunk from IO AES stream
 */
static ssize_t io_aes_read ( struct io_stream *io, void *data, size_t len )
{
    ssize_t aux;
    size_t limit;
    ssize_t peeklen;
    struct io_aes_context_t *context = ( struct io_aes_context_t * ) io->context;
    unsigned char encrypted[CHUNK];
    unsigned char peekarea[AES256_BLOCKLEN];

    if ( ( limit = sizeof ( context->unconsumed ) - context->u_len ) % AES256_BLOCKLEN )
    {
        limit -= limit % AES256_BLOCKLEN;
    }

    if ( ( aux = read ( context->fd, encrypted, limit ) ) < 0 )
    {
        return -1;
    }

    if ( ( peeklen = read ( context->fd, peekarea, sizeof ( peekarea ) ) ) < 0 )
    {
        return -1;
    }

    if ( peeklen && lseek ( context->fd, -peeklen, SEEK_CUR ) < 0 )
    {
        return -1;
    }

    if ( mbedtls_aes_crypt_cbc ( &context->aes, MBEDTLS_AES_DECRYPT, aux, context->iv, encrypted,
            context->unconsumed + context->u_len ) != 0 )
    {
        return -1;
    }

    context->u_len += aux;

    if ( peeklen < AES256_BLOCKLEN )
    {
        context->eof = 1;
    }

    limit = context->u_len;

    if ( context->eof )
    {
        limit -= AES256_BLOCKLEN - context->unaligned;
    }

    if ( len > limit )
    {
        len = limit;
    }

    memcpy ( data, context->unconsumed, len );
    memmove ( context->unconsumed, context->unconsumed + len, context->u_len - len );
    context->u_len -= len;

    if ( mbedtls_md_hmac_update ( &context->md_ctx, data, len ) != 0 )
    {
        return -1;
    }

    return len;
}

/*
 * Read complete data chunk from IO AES stream
 */
static int io_aes_read_complete ( struct io_stream *io, void *data, size_t len )
{
    size_t aux;
    size_t sum;

    for ( sum = 0; sum < len; sum += aux )
    {
        if ( ( ssize_t ) ( aux =
                io_aes_read ( io, ( unsigned char * ) data + sum, len - sum ) ) <= 0 )
        {
            return -1;
        }
    }

    return 0;
}

/*
 * Write complete data chunk to IO AES stream
 */
static int io_aes_write ( struct io_stream *io, const void *data, size_t len )
{
    size_t aux;
    size_t aligned;
    struct io_aes_context_t *context = ( struct io_aes_context_t * ) io->context;
    unsigned char encrypted[CHUNK];

    if ( ( aux = sizeof ( context->unconsumed ) - context->u_len ) > len )
    {
        aux = len;
    }

    memcpy ( context->unconsumed + context->u_len, data, aux );
    context->u_len += aux;

    aligned = context->u_len - context->u_len % AES256_BLOCKLEN;

    if ( mbedtls_aes_crypt_cbc ( &context->aes, MBEDTLS_AES_ENCRYPT, aligned, context->iv,
            context->unconsumed, encrypted ) != 0
        || write_complete ( context->fd, encrypted, aligned ) < 0 )
    {
        return -1;
    }

    context->u_len %= AES256_BLOCKLEN;
    memmove ( context->unconsumed, context->unconsumed + aligned, context->u_len );

    if ( mbedtls_md_hmac_update ( &context->md_ctx, data, aux ) != 0 )
    {
        return -1;
    }

    return aux;
}

/*
 * Write complete data chunk to IO AES stream
 */
static int io_aes_write_complete ( struct io_stream *io, const void *data, size_t len )
{
    size_t aux;
    size_t sum;

    for ( sum = 0; sum < len; sum += aux )
    {
        if ( ( ssize_t ) ( aux =
                io_aes_write ( io, ( const unsigned char * ) data + sum, len - sum ) ) <= 0 )
        {
            return -1;
        }
    }

    return 0;
}

/*
 * Flush IO AES stream
 */
static int io_aes_flush ( struct io_stream *io )
{
    struct io_aes_context_t *context = ( struct io_aes_context_t * ) io->context;
    unsigned char plaintext[AES256_BLOCKLEN] = { 0 };
    unsigned char encrypted[AES256_BLOCKLEN];

    if ( context->u_len >= AES256_BLOCKLEN )
    {
        return -1;
    }

    context->salt[0] = ( context->u_len << 4 ) | ( context->salt[0] & 0x0f );
    memcpy ( plaintext, context->unconsumed, context->u_len );

    if ( mbedtls_aes_crypt_cbc ( &context->aes, MBEDTLS_AES_ENCRYPT, AES256_BLOCKLEN, context->iv,
            plaintext, encrypted ) != 0
        || write_complete ( context->fd, encrypted, AES256_BLOCKLEN ) < 0
        || lseek ( context->fd, 0, SEEK_SET )
        || write_complete ( context->fd, context->salt, sizeof ( context->salt ) ) < 0
        || mbedtls_md_hmac_finish ( &context->md_ctx, context->hmac ) != 0
        || write_complete ( context->fd, context->hmac, sizeof ( context->hmac ) ) < 0 )
    {
        return -1;
    }

    return syncfs ( context->fd ) < 0 ? -1 : 0;
}

/*
 * Verify IO AES stream
 */
static int io_aes_verify ( struct io_stream *io )
{
    struct io_aes_context_t *context = ( struct io_aes_context_t * ) io->context;
    unsigned char hmac[SHA256_BLOCKLEN];

    if ( mbedtls_md_hmac_finish ( &context->md_ctx, hmac ) != 0 )
    {
        return -1;
    }

    return !memcmp ( hmac, context->hmac, SHA256_BLOCKLEN ) ? 1 : -1;
}

/*
 * Close IO AES stream
 */
static void io_aes_close ( struct io_stream *io )
{
    struct io_aes_context_t *context = ( struct io_aes_context_t * ) io->context;

    if ( io )
    {
        close ( context->fd );
        mbedtls_md_free ( &context->md_ctx );
        memset ( &context->aes, '\0', sizeof ( context->aes ) );
        free ( io );
    }
}

/**
 * Assign file descriptior with IO AES input stream
 */
struct io_stream *io_aes_istream_new ( int fd, const char *password )
{
    struct io_stream *io;
    struct io_aes_context_t *context;
    mbedtls_md_type_t md_type = MBEDTLS_MD_SHA256;
    unsigned char key[AES256_KEYLEN];

    if ( !( context =
            ( struct io_aes_context_t * ) calloc ( 1, sizeof ( struct io_aes_context_t ) ) ) )
    {
        return NULL;
    }

    context->fd = fd;
    context->eof = 0;
    context->u_len = 0;

    if ( read_complete ( context->fd, context->salt, sizeof ( context->salt ) ) < 0
        || read_complete ( context->fd, context->hmac, sizeof ( context->hmac ) ) < 0
        || read_complete ( context->fd, context->iv, sizeof ( context->iv ) ) < 0 )
    {
        free ( context );
        return NULL;
    }

    context->unaligned = ( context->salt[0] & 0xf0 ) >> 4;
    context->salt[0] &= 0x0f;

    if ( pbkdf2_sha256_derive_key ( password, context->salt, sizeof ( context->salt ), key,
            sizeof ( key ) ) < 0 )
    {
        memset ( key, '\0', sizeof ( key ) );
        free ( context );
        return NULL;
    }

    mbedtls_aes_init ( &context->aes );

    if ( mbedtls_aes_setkey_dec ( &context->aes, key, AES256_KEYLEN_BITS ) != 0 )
    {
        memset ( key, '\0', sizeof ( key ) );
        memset ( &context->aes, '\0', sizeof ( context->aes ) );
        free ( context );
        return NULL;
    }

    mbedtls_md_init ( &context->md_ctx );

    if ( mbedtls_md_setup ( &context->md_ctx, mbedtls_md_info_from_type ( md_type ), 1 ) != 0
        || mbedtls_md_hmac_starts ( &context->md_ctx, key, sizeof ( key ) ) != 0 )
    {
        memset ( key, '\0', sizeof ( key ) );
        memset ( &context->aes, '\0', sizeof ( context->aes ) );
        mbedtls_md_free ( &context->md_ctx );
        free ( context );
        return NULL;
    }

    memset ( key, '\0', sizeof ( key ) );

    if ( !( io = ( struct io_stream * ) calloc ( 1, sizeof ( struct io_stream ) ) ) )
    {
        memset ( &context->aes, '\0', sizeof ( context->aes ) );
        free ( context );
        return NULL;
    }

    io->context = ( struct io_base_context_t * ) context;
    io->read = io_aes_read;
    io->read_complete = io_aes_read_complete;
    io->verify = io_aes_verify;
    io->close = io_aes_close;

    return io;
}

/**
 * Assign file descriptior with IO AES output stream
 */
struct io_stream *io_aes_ostream_new ( int fd, const char *password )
{
    struct io_stream *io;
    struct io_aes_context_t *context;
    mbedtls_md_type_t md_type = MBEDTLS_MD_SHA256;
    unsigned char key[AES256_KEYLEN];
    unsigned char zeros[AES256_KEYLEN + SHA256_BLOCKLEN] = { 0 };

    if ( !( context =
            ( struct io_aes_context_t * ) calloc ( 1, sizeof ( struct io_aes_context_t ) ) ) )
    {
        return NULL;
    }

    context->fd = fd;
    context->eof = 0;
    context->u_len = 0;

    if ( random_bytes ( context->salt, sizeof ( context->salt ) ) < 0
        || random_bytes ( context->iv, sizeof ( context->iv ) ) < 0 )
    {
        free ( context );
        return NULL;
    }

    context->salt[0] &= 0x0f;

    if ( write_complete ( context->fd, zeros, sizeof ( zeros ) ) < 0
        || write_complete ( context->fd, context->iv, sizeof ( context->iv ) ) < 0 )
    {
        free ( context );
        return NULL;
    }

    if ( pbkdf2_sha256_derive_key ( password, context->salt, sizeof ( context->salt ), key,
            sizeof ( key ) ) < 0 )
    {
        memset ( key, '\0', sizeof ( key ) );
        free ( context );
        return NULL;
    }

    mbedtls_aes_init ( &context->aes );

    if ( mbedtls_aes_setkey_enc ( &context->aes, key, AES256_KEYLEN_BITS ) != 0 )
    {
        memset ( key, '\0', sizeof ( key ) );
        memset ( &context->aes, '\0', sizeof ( context->aes ) );
        free ( context );
        return NULL;
    }

    mbedtls_md_init ( &context->md_ctx );

    if ( mbedtls_md_setup ( &context->md_ctx, mbedtls_md_info_from_type ( md_type ), 1 ) != 0
        || mbedtls_md_hmac_starts ( &context->md_ctx, key, sizeof ( key ) ) != 0 )
    {
        memset ( key, '\0', sizeof ( key ) );
        memset ( &context->aes, '\0', sizeof ( context->aes ) );
        mbedtls_md_free ( &context->md_ctx );
        free ( context );
        return NULL;
    }

    memset ( key, '\0', sizeof ( key ) );

    if ( !( io = ( struct io_stream * ) calloc ( 1, sizeof ( struct io_stream ) ) ) )
    {
        memset ( &context->aes, '\0', sizeof ( context->aes ) );
        free ( context );
        return NULL;
    }

    io->context = ( struct io_base_context_t * ) context;
    io->write_complete = io_aes_write_complete;
    io->flush = io_aes_flush;
    io->close = io_aes_close;

    return io;
}
#endif
