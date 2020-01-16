/* ------------------------------------------------------------------
 * sbox - Simple Data Achive Utility
 * ------------------------------------------------------------------ */

#include "sbox.h"

/**
 * Show operation progress with current file path
 */
void show_progress ( char action, const char *path )
{
    printf ( " %c %s\n", action, path );
}

/**
 * Read complete data chunk from file
 */
int read_complete ( int fd, uint8_t * mem, size_t total )
{
    size_t len;
    size_t sum;

    for ( sum = 0; sum < total; sum += len )
    {
        if ( ( ssize_t ) ( len = read ( fd, mem + sum, total - sum ) ) <= 0 )
        {
            return -1;
        }
    }

    return 0;
}

/**
 * Write complete data chunk to file
 */
int write_complete ( int fd, const uint8_t * mem, size_t total )
{
    size_t len;
    size_t sum;

    for ( sum = 0; sum < total; sum += len )
    {
        if ( ( ssize_t ) ( len = write ( fd, mem + sum, total - sum ) ) <= 0 )
        {
            return -1;
        }
    }

    return 0;
}

/**
 * Get random bytes
 */
int random_bytes ( void *buffer, size_t length )
{
    int fd;
    size_t sum;
    size_t len;

    if ( ( fd = open ( "/dev/random", O_RDONLY ) ) < 0 )
    {
        return -1;
    }

    for ( sum = 0; sum < length; sum += len )
    {
        if ( ( len = read ( fd, ( ( unsigned char * ) buffer ) + sum, length - sum ) ) <= 0 )
        {
            close ( fd );
            return -1;
        }
    }

    close ( fd );
    return 0;
}
