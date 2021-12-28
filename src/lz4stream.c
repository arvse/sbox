/* ------------------------------------------------------------------
 * SBox - Simple Data Achive Utility
 * ------------------------------------------------------------------ */

#include "sbox.h"
#include <lz4frame.h>

#ifdef ENABLE_LZ4

#ifndef LZ4F_HEADER_SIZE_MAX
#define LZ4F_HEADER_SIZE_MAX 15
#endif

/**
 * LZ4 capable stream structure
 */
struct lz4_stream_context_t
{
    struct io_stream *io;
    int lz4_allocated;
    int begin_flag;
    unsigned char *unconsumed;
    size_t u_off;
    size_t u_len;
    size_t u_size;
    unsigned char *output;
    size_t capacity;
    LZ4F_preferences_t lz4_prefs;
    LZ4F_compressionContext_t lz4_ctx;
    LZ4F_decompressionContext_t lz4_dctx;
};

/**
 * Begin LZ4 stream compression
 */
static int lz4_begin ( struct ar_ostream *stream )
{
    size_t length;
    struct lz4_stream_context_t *context = ( struct lz4_stream_context_t * ) stream->context;
    unsigned char header[LZ4F_HEADER_SIZE_MAX];

    length =
        LZ4F_compressBegin ( context->lz4_ctx, header, sizeof ( header ), &context->lz4_prefs );

    if ( LZ4F_isError ( length ) )
    {
        return -1;
    }

    if ( stream->context->io->write_complete ( stream->context->io, header, length ) < 0 )
    {
        return -1;
    }

    context->begin_flag = 0;
    return 0;
}

/**
 * Write data to LZ4 output stream
 */
static int lz4_write ( struct ar_ostream *stream, const void *data, size_t len )
{
    size_t ilen;
    size_t olen;
    struct lz4_stream_context_t *context = ( struct lz4_stream_context_t * ) stream->context;

    if ( context->begin_flag && lz4_begin ( stream ) < 0 )
    {
        return -1;
    }

    while ( len )
    {
        ilen = len < CHUNK ? len : CHUNK;
        olen =
            LZ4F_compressUpdate ( context->lz4_ctx, context->output, context->capacity, data, ilen,
            NULL );

        if ( LZ4F_isError ( olen ) )
        {
            return -1;
        }

        if ( olen &&
            stream->context->io->write_complete ( stream->context->io, context->output, olen ) < 0 )
        {
            return -1;
        }

        data += ilen;
        len -= ilen;
    }

    return 0;
}

/**
 * Decompress data into context unconsumed data buffer
 */
static int decompress_data ( const unsigned char *input, size_t avail,
    struct lz4_stream_context_t *context )
{
    size_t ret;
    size_t ilen;
    size_t olen;
    unsigned char *u_backup;

    context->u_off = 0;
    context->u_len = 0;

    while ( avail )
    {
        if ( context->u_len + CHUNK > context->u_size )
        {
            u_backup = context->unconsumed;
            context->u_size <<= 1;
            if ( !( context->unconsumed =
                    ( unsigned char * ) realloc ( context->unconsumed, context->u_size ) ) )
            {
                perror ( "realloc" );
                free ( u_backup );
                return -1;
            }
        }

        ilen = avail;
        olen = CHUNK;

        ret =
            LZ4F_decompress ( context->lz4_dctx, context->unconsumed + context->u_len, &olen, input,
            &ilen, NULL );

        if ( LZ4F_isError ( ret ) )
        {
            return -1;
        }

        context->u_len += olen;
        input += ilen;
        avail -= ilen;
    }

    context->u_off = 0;

    return 0;
}

/**
 * Read data from zlib input stream
 */
static int lz4_read ( struct ar_istream *stream, void *data, size_t len )
{
    size_t have;
    size_t avail;
    struct lz4_stream_context_t *context = ( struct lz4_stream_context_t * ) stream->context;
    unsigned char input[CHUNK];

    if ( context->u_off < context->u_len )
    {
        have = context->u_len - context->u_off;
        if ( len < have )
        {
            have = len;
        }
        memcpy ( data, context->unconsumed + context->u_off, have );
        context->u_off += have;
        data += have;
        len -= have;
    }

    while ( len )
    {
        if ( ( ssize_t ) ( avail =
                stream->context->io->read ( stream->context->io, input, sizeof ( input ) ) ) <= 0 )
        {
            return -1;
        }

        if ( decompress_data ( input, avail, context ) < 0 )
        {
            return -1;
        }

        have = context->u_len < len ? context->u_len : len;
        memcpy ( data, context->unconsumed, have );
        context->u_off += have;
        data += have;
        len -= have;
    }

    return 0;
}

/*
 * Finalize zlib output stream
 */
static int lz4_flush ( struct ar_ostream *stream )
{
    size_t olen;
    struct lz4_stream_context_t *context = ( struct lz4_stream_context_t * ) stream->context;

    olen = LZ4F_compressEnd ( context->lz4_ctx, context->output, context->capacity, NULL );

    if ( LZ4F_isError ( olen ) )
    {
        return -1;
    }

    if ( olen &&
        stream->context->io->write_complete ( stream->context->io, context->output, olen ) < 0 )
    {
        return -1;
    }

    return context->io->flush ( context->io );
}

/*
 * Close LZ4 stream
 */
static void lz4_close ( struct ar_stream *stream )
{
    struct lz4_stream_context_t *context = ( struct lz4_stream_context_t * ) stream->context;

    if ( context->unconsumed )
    {
        free ( context->unconsumed );
        context->unconsumed = NULL;
    }

    if ( context->lz4_allocated )
    {
        if ( context->lz4_allocated == 1 )
        {
            LZ4F_freeCompressionContext ( context->lz4_ctx );

        } else if ( context->lz4_allocated == 2 )
        {
            LZ4F_freeDecompressionContext ( context->lz4_dctx );
        }
        context->lz4_allocated = 0;
    }

    generic_close ( stream );
}

/**
 * Open zlib output stream
 */
struct ar_ostream *lz4_ostream_open ( struct io_stream *io, int level )
{
    struct ar_ostream *stream;
    struct lz4_stream_context_t *context;
    LZ4F_preferences_t lz4_prefs = {
        {LZ4F_max256KB, LZ4F_blockLinked, LZ4F_noContentChecksum, LZ4F_frame,
            0 /* unknown content size */ , 0 /* no dictID */ , /* LZ4F_noBlockChecksum */ 0},
        level,  /* compression level; 0 == default */
        0,      /* autoflush */
        0,      /* favor decompression speed */
        {0, 0, 0},      /* reserved, must be set to 0 */
    };

    if ( !( stream = ( struct ar_ostream * ) calloc ( 1, sizeof ( struct ar_ostream ) ) ) )
    {
        return NULL;
    }

    if ( !( context =
            ( struct lz4_stream_context_t * ) calloc ( 1, sizeof ( struct
                    lz4_stream_context_t ) ) ) )
    {
        free ( stream );
        return NULL;
    }

    context->begin_flag = 1;

    stream->context = ( struct stream_base_context_t * ) context;
    stream->put_header = generic_put_header;
    stream->write = lz4_write;
    stream->flush = lz4_flush;
    stream->close = ( void ( * )( struct ar_ostream * ) ) lz4_close;
    memcpy ( &context->lz4_prefs, &lz4_prefs, sizeof ( LZ4F_preferences_t ) );
    context->lz4_allocated = 0;

    /* Unconsumed data buffer not allocated yet */
    context->unconsumed = NULL;

    /* Open stream as generic */
    generic_ostream_open ( stream->context, io );

    /* Prepare LZ4 compression context */
    if ( LZ4F_isError ( LZ4F_createCompressionContext ( &context->lz4_ctx, LZ4F_VERSION ) ) )
    {
        stream->close ( stream );
        return NULL;
    }

    /* Obtain compression bound */
    context->capacity = LZ4F_compressBound ( CHUNK, &context->lz4_prefs );

    /* Allocate compression output buffer */
    if ( !( context->output = ( unsigned char * ) malloc ( context->capacity ) ) )
    {
        LZ4F_freeCompressionContext ( context->lz4_ctx );
        stream->close ( stream );
        return NULL;
    }

    context->lz4_allocated = 1;

    return stream;
}

/**
 * Open zlib input stream
 */
struct ar_istream *lz4_istream_open ( struct io_stream *io )
{
    struct ar_istream *stream;
    struct lz4_stream_context_t *context;

    if ( !( stream = ( struct ar_istream * ) calloc ( 1, sizeof ( struct ar_istream ) ) ) )
    {
        return NULL;
    }

    if ( !( context =
            ( struct lz4_stream_context_t * ) calloc ( 1, sizeof ( struct
                    lz4_stream_context_t ) ) ) )
    {
        free ( stream );
        return NULL;
    }

    stream->context = ( struct stream_base_context_t * ) context;
    stream->get_header = generic_get_header;
    stream->read = lz4_read;
    stream->verify = generic_verify;
    stream->close = ( void ( * )( struct ar_istream * ) ) lz4_close;
    context->lz4_allocated = 0;

    /* Open stream as generic */
    generic_istream_open ( stream->context, io );

    /* Unconsumed data buffer not allocated yet */
    context->unconsumed = NULL;

    /* Prepare LZ4 decompression context */
    if ( LZ4F_isError ( LZ4F_createDecompressionContext ( &context->lz4_dctx, LZ4F_VERSION ) ) )
    {
        stream->close ( stream );
        return NULL;
    }

    context->u_off = 0;
    context->u_len = 0;
    context->u_size = 4 * CHUNK;

    if ( !( context->unconsumed = ( unsigned char * ) malloc ( context->u_size ) ) )
    {
        LZ4F_freeDecompressionContext ( context->lz4_dctx );
        stream->close ( stream );
        return NULL;
    }

    context->lz4_allocated = 2;

    return stream;
}

#endif
