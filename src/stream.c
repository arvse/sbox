/* ------------------------------------------------------------------
 * SBox - Simple Data Achive Utility
 * ------------------------------------------------------------------ */

#include "sbox.h"

/**
 * Open generic output stream
 */
void generic_ostream_open ( struct stream_base_context_t *context, struct io_stream *io )
{
    context->io = io;
}

/**
 * Open generic input stream
 */
void generic_istream_open ( struct stream_base_context_t *context, struct io_stream *io )
{
    context->io = io;
}

/**
 * Archive header host to network byte order change
 */
static void header_hton ( const struct header_t *header, struct header_t *net_header )
{
    memset ( net_header, '\0', sizeof ( struct header_t ) );
    memcpy ( net_header->magic, header->magic, sizeof ( net_header->magic ) );
    net_header->comp = htonl ( header->comp );
    net_header->nentity = htonl ( header->nentity );
    net_header->nameslen = htonl ( header->nameslen );
}

/**
 * Archive header network to host byte order change
 */
static void header_ntoh ( const struct header_t *net_header, struct header_t *header )
{
    memset ( header, '\0', sizeof ( struct header_t ) );
    memcpy ( header->magic, net_header->magic, sizeof ( header->magic ) );
    header->comp = ntohl ( net_header->comp );
    header->nentity = ntohl ( net_header->nentity );
    header->nameslen = ntohl ( net_header->nameslen );
}

/** 
 * Generic put archive header
 */
int generic_put_header ( struct ar_ostream *stream, const struct header_t *header )
{
    struct header_t net_header;

    header_hton ( header, &net_header );

    if ( stream->context->io->write_complete ( stream->context->io, &net_header,
            sizeof ( net_header ) ) < 0 )
    {
        return -1;
    }

    return 0;
}

/** 
 * Generic get archive header
 */
int generic_get_header ( struct ar_istream *stream, struct header_t *header )
{
    struct header_t net_header;

    if ( stream->context->io->read_complete ( stream->context->io, &net_header,
            sizeof ( net_header ) ) < 0 )
    {
        return -1;
    }

    header_ntoh ( &net_header, header );

    return 0;
}

/**
 * Write data to output stream
 */
int generic_write ( struct ar_ostream *stream, const void *data, size_t len )
{
    return stream->context->io->write_complete ( stream->context->io, data, len );
}

/**
 * Read data from input stream
 */
int generic_read ( struct ar_istream *stream, void *data, size_t len )
{
    return stream->context->io->read_complete ( stream->context->io, data, len );
}

/**
 * Verify full content read from input stream
 */
int generic_verify ( struct ar_istream *stream )
{
    return stream->context->io->verify ( stream->context->io );
}

/*
 * Finalize output stream
 */
int generic_flush ( struct ar_ostream *stream )
{
    return stream->context->io->flush ( stream->context->io );
}

/*
 * Close stream
 */
void generic_close ( struct ar_stream *stream )
{
    if ( stream )
    {
        free ( stream->context );
        free ( stream );
    }
}

/**
 * Open plain output stream
 */
struct ar_ostream *plain_ostream_open ( struct io_stream *io )
{
    struct ar_ostream *stream;

    if ( !( stream = ( struct ar_ostream * ) calloc ( 1, sizeof ( struct ar_ostream ) ) ) )
    {
        return NULL;
    }

    if ( !( stream->context =
            ( struct stream_base_context_t * ) malloc ( sizeof ( struct
                    stream_base_context_t ) ) ) )
    {
        free ( stream );
        return NULL;
    }

    stream->put_header = generic_put_header;
    stream->write = generic_write;
    stream->flush = generic_flush;
    stream->close = ( void ( * )( struct ar_ostream * ) ) generic_close;
    generic_ostream_open ( stream->context, io );

    return stream;
}

/**
 * Open plain input stream
 */
struct ar_istream *plain_istream_open ( struct io_stream *io )
{
    struct ar_istream *stream;

    if ( !( stream = ( struct ar_istream * ) calloc ( 1, sizeof ( struct ar_istream ) ) ) )
    {
        return NULL;
    }

    if ( !( stream->context =
            ( struct stream_base_context_t * ) calloc ( 1, sizeof ( struct
                    stream_base_context_t ) ) ) )
    {
        free ( stream );
        return NULL;
    }

    stream->get_header = generic_get_header;
    stream->read = generic_read;
    stream->verify = generic_verify;
    stream->close = ( void ( * )( struct ar_istream * ) ) generic_close;
    generic_istream_open ( stream->context, io );

    return stream;
}
