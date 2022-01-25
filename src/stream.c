/* ------------------------------------------------------------------
 * SBox - Creation of New Streams
 * ------------------------------------------------------------------ */

#include "sbox.h"

/**
 * Create new IO stream
 */
struct io_stream_t *io_stream_new ( void )
{
    struct io_stream_t *io;

    if ( !( io = ( struct io_stream_t * ) calloc ( 1, sizeof ( struct io_stream_t ) ) ) )
    {
        return NULL;
    }

    io->read_complete = stream_read_complete;
    io->read_max = stream_read_max;
    io->write_complete = stream_write_complete;

    return io;
}

/**
 * Create new input stream
 */
struct io_stream_t *input_stream_new ( int fd, const char *password )
{
    uint8_t compression;
    struct io_stream_t *file_stream;
    struct io_stream_t *storage_stream;
#ifdef ENABLE_LZ4
    struct io_stream_t *inflate_stream;
#endif
    struct io_stream_t *stream;
    struct io_stream_t *buffer_stream;
    unsigned char prefix[ARCHIVE_PREFIX_LENGTH];

    if ( !( file_stream = file_stream_new ( fd ) ) )
    {
        return NULL;
    }

    if ( password )
    {
#ifdef ENABLE_ENCRYPTION
        if ( !( storage_stream = input_aes_stream_new ( file_stream, password ) ) )
        {
            file_stream->close ( file_stream );
            return NULL;
        }
#else
        UNUSED ( password );
        fprintf ( stderr, "Error: Crypto support not enabled.\n" );
        file_stream->close ( file_stream );
        errno = ENOTSUP;
        return NULL;
#endif
    } else
    {
        storage_stream = file_stream;
    }

    if ( storage_stream->read_complete ( storage_stream, prefix, sizeof ( prefix ) ) < 0 )
    {
        storage_stream->close ( storage_stream );
        return NULL;
    }

    if ( memcmp ( prefix, sbox_archive_prefix, ARCHIVE_PREFIX_LENGTH ) != 0 )
    {
        fprintf ( stderr, "Error: Archive not recognized.\n" );
        storage_stream->close ( storage_stream );
        errno = EINVAL;
        return NULL;
    }

    if ( storage_stream->read_complete ( storage_stream, &compression,
            sizeof ( compression ) ) < 0 )
    {
        storage_stream->close ( storage_stream );
        return NULL;
    }

    switch ( compression )
    {
    case COMP_NONE:
        stream = storage_stream;
        break;
    case COMP_LZ4:
#ifdef ENABLE_LZ4
        if ( !( inflate_stream = input_lz4_stream_new ( storage_stream ) ) )
        {
            storage_stream->close ( storage_stream );
            return NULL;
        }

        stream = inflate_stream;
        break;
#else
        fprintf ( stderr, "Error: Compression support not enabled.\n" );
        storage_stream->close ( storage_stream );
        errno = ENOTSUP;
        return NULL;
#endif
    default:
        fprintf ( stderr, "Error: Unknown compression mode requested.\n" );
        storage_stream->close ( storage_stream );
        errno = ENOTSUP;
        return NULL;
    }

    if ( !( buffer_stream = buffer_stream_new ( stream ) ) )
    {
        stream->close ( stream );
        return NULL;
    }

    return buffer_stream;
}

/**
 * Create new output stream
 */
struct io_stream_t *output_stream_new ( int fd, const char *password, uint8_t compression,
    int level )
{
    struct io_stream_t *file_stream;
    struct io_stream_t *storage_stream;
#ifdef ENABLE_LZ4
    struct io_stream_t *deflate_stream;
#endif
    struct io_stream_t *stream;
    struct io_stream_t *buffer_stream;

    if ( !( file_stream = file_stream_new ( fd ) ) )
    {
        return NULL;
    }

    if ( password )
    {
#ifdef ENABLE_ENCRYPTION
        if ( !( storage_stream = output_aes_stream_new ( file_stream, password ) ) )
        {
            file_stream->close ( file_stream );
            return NULL;
        }
#else
        UNUSED ( password );
        fprintf ( stderr, "Error: Crypto support not enabled.\n" );
        file_stream->close ( file_stream );
        errno = ENOTSUP;
        return NULL;
#endif
    } else
    {
        storage_stream = file_stream;
    }

    if ( storage_stream->write_complete ( storage_stream, sbox_archive_prefix,
            sizeof ( sbox_archive_prefix ) ) < 0 )
    {
        storage_stream->close ( storage_stream );
        return NULL;
    }

    if ( storage_stream->write_complete ( storage_stream, &compression,
            sizeof ( compression ) ) < 0 )
    {
        storage_stream->close ( storage_stream );
        return NULL;
    }

    switch ( compression )
    {
    case COMP_NONE:
        stream = storage_stream;
        break;
    case COMP_LZ4:
#ifdef ENABLE_LZ4
        if ( !( deflate_stream = output_lz4_stream_new ( storage_stream, level ) ) )
        {
            storage_stream->close ( storage_stream );
            return NULL;
        }

        stream = deflate_stream;
        break;
#else
        UNUSED ( level );
        fprintf ( stderr, "Error: Compression support not enabled.\n" );
        storage_stream->close ( storage_stream );
        errno = ENOTSUP;
        return NULL;
#endif
    default:
        fprintf ( stderr, "Error: Unknown compression mode requested.\n" );
        storage_stream->close ( storage_stream );
        errno = ENOTSUP;
        return NULL;
    }

    if ( !( buffer_stream = buffer_stream_new ( stream ) ) )
    {
        stream->close ( stream );
        return NULL;
    }

    return buffer_stream;
}

/**
 * Read complete data chunk from stream
 */
int stream_read_complete ( struct io_stream_t *io, void *mem, size_t total )
{
    size_t len;
    size_t sum;

    for ( sum = 0; sum < total; sum += len )
    {
        if ( ( ssize_t ) ( len = io->read ( io, ( uint8_t * ) mem + sum, total - sum ) ) < 0 )
        {
            return -1;
        }

        if ( !len )
        {
            errno = ENODATA;
            return -1;
        }
    }

    return 0;
}

/**
 * Read longest data chunk from stream
 */
ssize_t stream_read_max ( struct io_stream_t *io, void *mem, size_t total )
{
    size_t len;
    size_t sum;

    for ( sum = 0; sum < total; sum += len )
    {
        if ( ( ssize_t ) ( len = io->read ( io, ( uint8_t * ) mem + sum, total - sum ) ) < 0 )
        {
            return -1;
        }

        if ( !len )
        {
            break;
        }
    }

    return sum;
}

/**
 * Write complete data chunk to stream
 */
int stream_write_complete ( struct io_stream_t *io, const void *mem, size_t total )
{
    size_t len;
    size_t sum;

    for ( sum = 0; sum < total; sum += len )
    {
        if ( ( ssize_t ) ( len =
                io->write ( io, ( const uint8_t * ) mem + sum, total - sum ) ) <= 0 )
        {
            return -1;
        }
    }

    return 0;
}
