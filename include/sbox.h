/* ------------------------------------------------------------------
 * SBox - Project Shared Header
 * ------------------------------------------------------------------ */

#include "config.h"

#ifndef SBOX_H
#define SBOX_H

#define SBOX_VERSION "2.0.11"

#define COMP_NONE 0
#define COMP_LZ4 1

#define ARCHIVE_PREFIX_LENGTH 4

#define OPTION_VERBOSE 1
#define OPTION_LISTONLY 2
#define OPTION_TESTONLY 4
#define OPTION_LZ4 8

/**
 * SBox Archive Node
 */
struct sbox_node_t
{
    uint32_t mode;
    time_t mtime;
    uint32_t size;
    char *name;
    struct sbox_node_t *head;
    struct sbox_node_t *tail;
    struct sbox_node_t *next;
    struct sbox_node_t *prev;
};

/**
 * SBox iterate context
 */
struct iter_context_t
{
    int options;
    struct io_stream_t *io;
    char buffer[CHUNK_SIZE];
};

/**
 * IO Stream context
 */
struct io_stream_t
{
    void *context;
      ssize_t ( *read ) ( struct io_stream_t *, void *, size_t );
      ssize_t ( *write ) ( struct io_stream_t *, const void *, size_t );
    int ( *read_complete ) ( struct io_stream_t *, void *, size_t );
      ssize_t ( *read_max ) ( struct io_stream_t *, void *, size_t );
    int ( *write_complete ) ( struct io_stream_t *, const void *, size_t );
    int ( *verify ) ( struct io_stream_t * );
    int ( *flush ) ( struct io_stream_t * );
    void ( *close ) ( struct io_stream_t * );
};

/**
 * File net browsing callback
 */
typedef int ( *file_net_iter_callback ) ( void *, struct sbox_node_t *, const char * );

/**
 * Pack files to an archive
 */
extern int sbox_pack_archive ( const char *archive, uint32_t options, int level,
    const char *password, const char *files[] );

/** 
 * Unpack files from an archive
 */
extern int sbox_unpack_archive ( const char *archive, uint32_t options, const char *password );

/**
 * Show operation progress with current file path
 */
extern void show_progress ( char action, const char *path );

/**
 * SBox archive prefix
 */
extern const unsigned char sbox_archive_prefix[ARCHIVE_PREFIX_LENGTH];

/**
 * Create new IO stream
 */
extern struct io_stream_t *io_stream_new ( void );

/**
 * Create new input stream
 */
extern struct io_stream_t *input_stream_new ( int fd, const char *password );

/**
 * Create new output stream
 */
extern struct io_stream_t *output_stream_new ( int fd, const char *password, uint8_t compression,
    int level );

/**
 * Read complete data chunk from stream
 */
extern int stream_read_complete ( struct io_stream_t *io, void *mem, size_t total );

/**
 * Read longest data chunk from stream
 */
extern ssize_t stream_read_max ( struct io_stream_t *io, void *mem, size_t total );

/**
 * Write complete data chunk to stream
 */
extern int stream_write_complete ( struct io_stream_t *io, const void *mem, size_t total );

/**
 * Create new file stream
 */
extern struct io_stream_t *file_stream_new ( int fd );

/**
 * Create new input AES stream
 */
#ifdef ENABLE_ENCRYPTION
extern struct io_stream_t *input_aes_stream_new ( struct io_stream_t *internal,
    const char *password );
#endif

/**
 * Create new output AES stream
 */
#ifdef ENABLE_ENCRYPTION
extern struct io_stream_t *output_aes_stream_new ( struct io_stream_t *internal,
    const char *password );
#endif

/**
 * Create new output LZ4 stream
 */
#ifdef ENABLE_LZ4
extern struct io_stream_t *output_lz4_stream_new ( struct io_stream_t *internal, int level );
#endif

/**
 * Create new input LZ4 stream
 */
#ifdef ENABLE_LZ4
extern struct io_stream_t *input_lz4_stream_new ( struct io_stream_t *internal );
#endif

/**
 * Create new buffer stream
 */
extern struct io_stream_t *buffer_stream_new ( struct io_stream_t *internal );

/**
 * Create new file net from paths
 */
struct sbox_node_t *build_file_net ( const char *paths[] );

/**
 * Browse file net
 */
extern int file_net_iter ( struct sbox_node_t *root, void *context,
    file_net_iter_callback callback );

/**
 * Free file net from memory
 */
extern void free_file_net ( struct sbox_node_t *node );

/**
 * Save file net to stream
 */
extern int file_net_save ( struct sbox_node_t *root, struct io_stream_t *io );

/**
 * Load file net from stream
 */
extern struct sbox_node_t *file_net_load ( struct io_stream_t *io );


#endif
