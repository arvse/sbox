/* ------------------------------------------------------------------
 * SBox - LZ4 Compressed Stream Impl.
 * ------------------------------------------------------------------ */

#include "sbox.h"
#include <lz4frame.h>

#ifdef ENABLE_LZ4

#ifndef LZ4F_HEADER_SIZE_MAX
#define LZ4F_HEADER_SIZE_MAX 15
#endif

/**
 * LZ4 stream context
 */
struct lz4_stream_context_t
{
    int begin_flag;

    size_t offset;
    size_t length;
    size_t capacity;

    uint8_t *buffer;

    struct io_stream_t *internal;
    LZ4F_preferences_t lz4_prefs;
    LZ4F_compressionContext_t lz4_ctx;
    LZ4F_decompressionContext_t lz4_dctx;

    uint8_t workbuf[MAX ( LZ4F_HEADER_SIZE_MAX, CHUNK_SIZE )];
};

/**
 * Dequeue data from cache buffer
 */
static size_t dequeue_buffer ( struct lz4_stream_context_t *context, void *data, size_t len )
{
    size_t dequeue_len;

    dequeue_len = MIN ( len, context->length - context->offset );
    memcpy ( data, context->buffer + context->offset, dequeue_len );
    context->offset += dequeue_len;

    return dequeue_len;
}

/**
 * Read data from AES stream
 */
static ssize_t lz4_stream_read ( struct io_stream_t *io, void *data, size_t len )
{
    int ret;
    size_t ilen;
    size_t olen;
    struct lz4_stream_context_t *context;

    context = ( struct lz4_stream_context_t * ) io->context;

    if ( context->offset < context->length )
    {
        return dequeue_buffer ( context, data, len );
    }

    do
    {
        if ( ( ssize_t ) ( ilen =
                context->internal->read_max ( context->internal, context->workbuf,
                    sizeof ( context->workbuf ) ) ) <= 0 )
        {
            return -1;
        }

        olen = context->capacity;

        ret =
            LZ4F_decompress ( context->lz4_dctx, context->buffer, &olen, context->workbuf, &ilen,
            NULL );

        if ( LZ4F_isError ( ret ) )
        {
            return -1;
        }

    } while ( !olen );

    context->offset = 0;
    context->length = olen;

    return dequeue_buffer ( context, data, len );
}

/**
 * Begin LZ4 stream compression
 */
static int lz4_stream_begin ( struct lz4_stream_context_t *context )
{
    size_t length;

    length =
        LZ4F_compressBegin ( context->lz4_ctx, context->workbuf, sizeof ( context->workbuf ),
        &context->lz4_prefs );

    if ( LZ4F_isError ( length ) )
    {
        return -1;
    }

    if ( context->internal->write_complete ( context->internal, context->workbuf, length ) < 0 )
    {
        return -1;
    }

    context->begin_flag = 0;
    return 0;
}

/**
 * Write data to LZ4 stream
 */
static ssize_t lz4_stream_write ( struct io_stream_t *io, const void *data, size_t len )
{
    size_t ilen;
    size_t olen;
    struct lz4_stream_context_t *context;

    context = ( struct lz4_stream_context_t * ) io->context;

    if ( context->begin_flag )
    {
        if ( lz4_stream_begin ( context ) < 0 )
        {
            return -1;
        }
    }

    ilen = MIN ( len, CHUNK_SIZE );
    olen =
        LZ4F_compressUpdate ( context->lz4_ctx, context->buffer, context->capacity, data, ilen,
        NULL );

    if ( LZ4F_isError ( olen ) )
    {
        return -1;
    }

    if ( context->internal->write_complete ( context->internal, context->buffer, olen ) < 0 )
    {
        return -1;
    }

    return ilen;
}

/*
 * Verify LZ4 stream integrity
 */
static int lz4_stream_verify ( struct io_stream_t *io )
{
    struct lz4_stream_context_t *context;

    context = ( struct lz4_stream_context_t * ) io->context;

    return context->internal->verify ( context->internal );
}

/*
 * Flush LZ4 stream output
 */
static int lz4_stream_flush ( struct io_stream_t *io )
{
    size_t olen;
    struct lz4_stream_context_t *context;

    context = ( struct lz4_stream_context_t * ) io->context;

    olen = LZ4F_compressEnd ( context->lz4_ctx, context->buffer, context->capacity, NULL );

    if ( LZ4F_isError ( olen ) )
    {
        return -1;
    }

    if ( context->internal->write_complete ( context->internal, context->buffer, olen ) < 0 )
    {
        return -1;
    }

    return context->internal->flush ( context->internal );
}

/*
 * Close LZ4 stream
 */
static void lz4_stream_close ( struct io_stream_t *io )
{
    struct lz4_stream_context_t *context;

    context = ( struct lz4_stream_context_t * ) io->context;

    if ( context->buffer )
    {
        free ( context->buffer );
    }

    if ( io->write )
    {
        LZ4F_freeCompressionContext ( context->lz4_ctx );
    }

    if ( io->read )
    {
        LZ4F_freeDecompressionContext ( context->lz4_dctx );
    }

    context->internal->close ( context->internal );
}

/**
 * Create new input LZ4 stream
 */
struct io_stream_t *input_lz4_stream_new ( struct io_stream_t *internal )
{
    struct io_stream_t *io;
    struct lz4_stream_context_t *context;

    if ( !( context =
            ( struct lz4_stream_context_t * ) calloc ( 1, sizeof ( struct
                    lz4_stream_context_t ) ) ) )
    {
        return NULL;
    }

    /* Initialize stream context */
    context->internal = internal;
    context->offset = 0;
    context->length = 0;

    /* Prepare LZ4 decompression context */
    if ( LZ4F_isError ( LZ4F_createDecompressionContext ( &context->lz4_dctx, LZ4F_VERSION ) ) )
    {
        free ( context );
        return NULL;
    }

    /* Obtain decompression bound */
    context->capacity = 256 * CHUNK_SIZE;

    /* Allocate compression buffer */
    if ( !( context->buffer = ( uint8_t * ) malloc ( context->capacity ) ) )
    {
        LZ4F_freeCompressionContext ( context->lz4_ctx );
        free ( context );
        return NULL;
    }

    if ( !( io = io_stream_new (  ) ) )
    {
        LZ4F_freeCompressionContext ( context->lz4_ctx );
        free ( context );
        return NULL;
    }

    io->context = ( struct io_base_context_t * ) context;
    io->read = lz4_stream_read;
    io->verify = lz4_stream_verify;
    io->close = lz4_stream_close;

    return io;
}

/**
 * Create new output LZ4 stream
 */
struct io_stream_t *output_lz4_stream_new ( struct io_stream_t *internal, int level )
{
    struct io_stream_t *io;
    struct lz4_stream_context_t *context;

    if ( !( context =
            ( struct lz4_stream_context_t * ) calloc ( 1, sizeof ( struct
                    lz4_stream_context_t ) ) ) )
    {
        return NULL;
    }

    /* Initialize stream context */
    context->begin_flag = 1;
    context->internal = internal;
    memset ( &context->lz4_prefs, 0, sizeof ( context->lz4_prefs ) );
    context->lz4_prefs.compressionLevel = level;

    /* Prepare LZ4 compression context */
    if ( LZ4F_isError ( LZ4F_createCompressionContext ( &context->lz4_ctx, LZ4F_VERSION ) ) )
    {
        free ( context );
        return NULL;
    }

    /* Obtain compression bound */
    context->capacity = LZ4F_compressBound ( CHUNK_SIZE, &context->lz4_prefs );

    /* Allocate compression buffer */
    if ( !( context->buffer = ( uint8_t * ) malloc ( context->capacity ) ) )
    {
        LZ4F_freeCompressionContext ( context->lz4_ctx );
        free ( context );
        return NULL;
    }

    if ( !( io = io_stream_new (  ) ) )
    {
        LZ4F_freeCompressionContext ( context->lz4_ctx );
        free ( context->buffer );
        free ( context );
        return NULL;
    }

    io->context = ( struct io_base_context_t * ) context;
    io->write = lz4_stream_write;
    io->flush = lz4_stream_flush;
    io->close = lz4_stream_close;

    return io;
}

#endif
