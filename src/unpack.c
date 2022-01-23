/* ------------------------------------------------------------------
 * SBox - Archive Unpack Task
 * ------------------------------------------------------------------ */

#include "sbox.h"

/**
 * SBox archive unpack callback
 */
int sbox_unpack_callback ( void *context, struct sbox_node_t *node, const char *path )
{
    int fd;
    size_t len;
    size_t sum = 0;
    struct io_stream_t *io;
    struct iter_context_t *iter_context;
    struct stat statbuf;

    iter_context = ( struct iter_context_t * ) context;

    if ( iter_context->options & OPTION_LISTONLY )
    {
        show_progress ( 'l', path );
        return 0;
    }

    if ( node->mode & S_IFDIR )
    {
        if ( iter_context->options & OPTION_TESTONLY )
        {
            show_progress ( 't', path );
            return 0;
        }

        if ( stat ( path, &statbuf ) >= 0 )
        {
            if ( statbuf.st_mode & S_IFDIR )
            {
                return 0;
            }

        } else
        {
            return mkdir ( path, node->mode );
        }
    }

    if ( iter_context->options & OPTION_TESTONLY )
    {
        if ( node->size )
        {
            do
            {
                len = MIN ( sizeof ( iter_context->buffer ), node->size - sum );

                if ( iter_context->io->read_complete ( iter_context->io, iter_context->buffer,
                        len ) < 0 )
                {
                    return -1;
                }

                sum += len;

            } while ( sum < node->size );
        }
        show_progress ( 't', path );
        return 0;
    }

    if ( ( fd = open ( path, O_CREAT | O_TRUNC | O_WRONLY | O_BINARY, node->mode ) ) < 0 )
    {
        perror ( path );
        return -1;
    }

    if ( !( io = file_stream_new ( fd ) ) )
    {
        close ( fd );
        return -1;
    }

    if ( node->size )
    {
        do
        {
            len = MIN ( sizeof ( iter_context->buffer ), node->size - sum );

            if ( iter_context->io->read_complete ( iter_context->io, iter_context->buffer,
                    len ) < 0 )
            {
                close ( fd );
                return -1;
            }

            if ( io->write_complete ( io, iter_context->buffer, len ) < 0 )
            {
                perror ( path );
                close ( fd );
                return -1;
            }

            sum += len;

        } while ( sum < node->size );
    }

    io->close ( io );

    if ( iter_context->options & OPTION_VERBOSE )
    {
        show_progress ( iter_context->options & OPTION_NOPATHS ? 'e' : 'x', path );
    }

    return 0;
}

/** 
 * Unpack files from an archive
 */
int sbox_unpack_archive ( const char *archive, uint32_t options, const char *password )
{
    int fd;
    int status = 0;
    struct io_stream_t *io;
    struct sbox_node_t *root;
    struct iter_context_t *iter_context;
    unsigned char prefix[ARCHIVE_PREFIX_LENGTH];

    if ( ( fd = open ( archive, O_RDONLY | O_BINARY ) ) < 0 )
    {
        perror ( archive );
        return -1;
    }

    if ( !( io = input_stream_new ( fd, password ) ) )
    {
        close ( fd );
        return -1;
    }

    if ( io->read_complete ( io, prefix, sizeof ( prefix ) ) < 0 )
    {
        io->close ( io );
        return -1;
    }

    if ( memcmp ( prefix, sbox_archive_prefix, ARCHIVE_PREFIX_LENGTH ) != 0 )
    {
        fprintf ( stderr, "Error: Archive not recognized.\n" );
        io->close ( io );
        errno = EINVAL;
        return -1;
    }

    if ( !( root = file_net_load ( io ) ) )
    {
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

    if ( file_net_iter ( root, iter_context, sbox_unpack_callback ) < 0 )
    {
        free ( iter_context );
        free_file_net ( root );
        io->close ( io );
        return -1;
    }

    free ( iter_context );
    free_file_net ( root );

    if ( ~options & OPTION_LISTONLY )
    {
        if ( io->verify ( io ) < 0 )
        {
            fprintf ( stderr, "archive checksum: bad\n" );
            errno = EINVAL;
            status = -1;

        } else
        {
            printf ( "archive checksum: ok\n" );
        }
    }

    io->close ( io );

    return status;
}
