/* ------------------------------------------------------------------
 * SBox - Simple Data Achive Utility
 * ------------------------------------------------------------------ */

#include "sbox.h"

/*
 * Read data chunk from IO stream
 */
static ssize_t io_read ( struct io_stream *io, void *data, size_t len )
{
    return read ( io->context->fd, data, len );
}

/*
 * Read complete data chunk from IO stream
 */
static int io_read_complete ( struct io_stream *io, void *data, size_t len )
{
    return read_complete ( io->context->fd, data, len );
}

/*
 * Write complete data chunk to IO stream
 */
static int io_write_complete ( struct io_stream *io, const void *data, size_t len )
{
    return write_complete ( io->context->fd, data, len );
}

/*
 * Verify IO stream
 */
static int io_verify ( struct io_stream *io )
{
    UNUSED ( io );
    return 0;
}

/*
 * Flush IO stream
 */
static int io_flush ( struct io_stream *io )
{
    return syncfs ( io->context->fd ) < 0 ? -1 : 0;
}

/*
 * Close IO stream
 */
static void io_close ( struct io_stream *io )
{
    if ( io )
    {
        close ( io->context->fd );
        free ( io );
    }
}

/**
 * Assign file descriptior with IO stream
 */
struct io_stream *io_stream_new ( int fd )
{
    struct io_stream *io;
    struct io_base_context_t *context;

    if ( !( io = ( struct io_stream * ) calloc ( 1, sizeof ( struct io_stream ) ) ) )
    {
        return NULL;
    }

    if ( !( context =
            ( struct io_base_context_t * ) calloc ( 1, sizeof ( struct io_base_context_t ) ) ) )
    {
        free ( io );
        return NULL;
    }

    context->fd = fd;

    io->context = context;
    io->read = io_read;
    io->read_complete = io_read_complete;
    io->write_complete = io_write_complete;
    io->verify = io_verify;
    io->flush = io_flush;
    io->close = io_close;

    return io;
}

/**
 * Assign file descriptior with input IO stream
 */
struct io_stream *io_istream_from ( int fd, const char *password )
{
    if ( password )
    {
#ifdef ENABLE_ENCRYPTION
        return io_aes_istream_new ( fd, password );
#else
        UNUSED ( password );
        fprintf ( stderr, "encryption not enabled.\n" );
        errno = ENOTSUP;
        return NULL;
#endif
    } else
    {
        return io_stream_new ( fd );
    }
}

/**
 * Assign file descriptior with output IO stream
 */
struct io_stream *io_ostream_from ( int fd, const char *password )
{
    if ( password )
    {
#ifdef ENABLE_ENCRYPTION
        return io_aes_ostream_new ( fd, password );
#else
        UNUSED ( password );
        fprintf ( stderr, "encryption not enabled.\n" );
        errno = ENOTSUP;
        return NULL;
#endif
    } else
    {
        return io_stream_new ( fd );
    }
}
