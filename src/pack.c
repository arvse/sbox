/* ------------------------------------------------------------------
 * SBox - Archive Pack Task
 * ------------------------------------------------------------------ */

#include "sbox.h"

#ifndef EXTRACT_ONLY

/**
 * SBox archive pack callback
 */
int sbox_pack_callback ( void *context, struct sbox_node_t *node, const char *path )
{
    int fd;
    size_t len;
    size_t sum = 0;
    struct io_stream_t *io;
    struct iter_context_t *iter_context;
    struct stat statbuf;

    iter_context = ( struct iter_context_t * ) context;

    if ( node->mode & S_IFDIR )
    {
        if ( stat ( path, &statbuf ) < 0 )
        {
            perror ( path );
            return -1;
        }

        if ( statbuf.st_mtime != node->mtime )
        {
            fprintf ( stderr, "Error: Directory '%s' has changed.\n", path );
            return -1;
        }

        return 0;
    }

    if ( ( fd = open ( path, O_RDONLY | O_BINARY ) ) < 0 )
    {
        perror ( path );
        return -1;
    }

    if ( fstat ( fd, &statbuf ) < 0 )
    {
        perror ( path );
        close ( fd );
        return -1;
    }

    if ( statbuf.st_mtime != node->mtime )
    {
        fprintf ( stderr, "Error: File '%s' has changed.\n", path );
        close ( fd );
        return -1;
    }

    if ( !( io = file_stream_new ( fd ) ) )
    {
        close ( fd );
        return -1;
    }

    while ( ( ssize_t ) ( len =
            io->read_max ( io, iter_context->buffer, sizeof ( iter_context->buffer ) ) ) > 0 )
    {
        if ( iter_context->io->write_complete ( iter_context->io, iter_context->buffer, len ) < 0 )
        {
            perror ( path );
            io->close ( io );
            return -1;
        }

        sum += len;
    }

    io->close ( io );

    if ( sum != node->size )
    {
        perror ( "read" );
        return -1;
    }

    if ( iter_context->options & OPTION_VERBOSE )
    {
        show_progress ( 'a', path );
    }

    return 0;
}

/**
 * Pack files to an archive
 */
int sbox_pack_archive ( const char *archive, uint32_t options, int level, const char *password,
    const char *files[] )
{
    int fd;
    int compression;
    struct io_stream_t *io;
    struct sbox_node_t *root;
    struct iter_context_t *iter_context;

    if ( ( fd = open ( archive, O_CREAT | O_TRUNC | O_WRONLY | O_BINARY, 0644 ) ) < 0 )
    {
        perror ( archive );
        return -1;
    }

    compression = ( options & OPTION_LZ4 ) ? COMP_LZ4 : 0;

    if ( !( io = output_stream_new ( fd, password, compression, level ) ) )
    {
        close ( fd );
        return -1;
    }

    if ( io->write_complete ( io, sbox_archive_prefix, sizeof ( sbox_archive_prefix ) ) < 0 )
    {
        io->close ( io );
        return -1;
    }

    if ( !( root = build_file_net ( files ) ) )
    {
        io->close ( io );
        return -1;
    }

    if ( file_net_save ( root, io ) < 0 )
    {
        free_file_net ( root );
        io->close ( io );
        return -1;
    }

    if ( !( iter_context =
            ( struct iter_context_t * ) malloc ( sizeof ( struct iter_context_t ) ) ) )
    {
        free_file_net ( root );
        io->close ( io );
        return -1;
    }

    iter_context->options = options;
    iter_context->io = io;

    if ( file_net_iter ( root, iter_context, sbox_pack_callback ) < 0 )
    {
        free ( iter_context );
        free_file_net ( root );
        io->close ( io );
        return -1;
    }

    free ( iter_context );
    free_file_net ( root );

    if ( io->flush ( io ) < 0 )
    {
        io->close ( io );
        return -1;
    }

    io->close ( io );

    return 0;
}

#endif
