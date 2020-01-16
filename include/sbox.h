/* ------------------------------------------------------------------
 * SBox - Simple Data Achive Utility
 * ------------------------------------------------------------------ */

#include "config.h"

#ifndef sbox_H
#define sbox_H

#define sbox_VERSION "1.0.16"

#define COMP_NONE 0
#define COMP_LZ4 1

#define OPTION_VERBOSE 1
#define OPTION_NOPATHS 2
#define OPTION_LISTONLY 4
#define OPTION_TESTONLY 8
#define OPTION_LZ4 16

struct header_t
{
    uint8_t magic[4];
    uint32_t comp;
    uint32_t nentity;
    uint32_t nameslen;
} __attribute__( ( packed ) );

struct entity_t
{
    uint32_t parent;
    uint32_t mode;
    union
    {
        uint32_t id;
        uint32_t size;
    };
} __attribute__( ( packed ) );

struct node_t
{
    char *name;
    struct node_t *next;
    struct node_t *sub;
    struct entity_t entity;
};

struct pack_context_t
{
    uint32_t options;
    struct ar_ostream *ostream;
    char path[PATH_LIMIT];
    unsigned char *workbuf;
    size_t workbuf_size;
};

struct unpack_context_t
{
    uint32_t options;
    struct ar_istream *istream;
    char path[PATH_LIMIT];
    unsigned char *workbuf;
    size_t workbuf_size;
};

struct scan_context_t
{
    uint32_t next_id;
    char path[PATH_LIMIT];
    char *filter;
};

struct unpack_parse_context
{
    const struct entity_t *entity_table;
    const char *name_table;
    const char *name_limit;
    uint32_t count;
    uint32_t position;
};

struct io_base_context_t
{
    int fd;
};

struct io_stream
{
    struct io_base_context_t *context;
      ssize_t ( *read ) ( struct io_stream *, void *, size_t );
    int ( *read_complete ) ( struct io_stream *, void *, size_t );
    int ( *write_complete ) ( struct io_stream *, const void *, size_t );
    int ( *verify ) ( struct io_stream * );
    int ( *flush ) ( struct io_stream * );
    void ( *close ) ( struct io_stream * );
};

struct stream_base_context_t
{
    struct io_stream *io;
};

struct ar_stream
{
    struct stream_base_context_t *context;
};

struct ar_ostream
{
    struct stream_base_context_t *context;
    int ( *put_header ) ( struct ar_ostream *, const struct header_t * );
    int ( *write ) ( struct ar_ostream *, const void *, size_t );
    int ( *flush ) ( struct ar_ostream * );
    void ( *close ) ( struct ar_ostream * );
};

struct ar_istream
{
    struct stream_base_context_t *context;
    int ( *get_header ) ( struct ar_istream *, struct header_t * );
    int ( *read ) ( struct ar_istream *, void *, size_t );
    int ( *verify ) ( struct ar_istream * );
    void ( *close ) ( struct ar_istream * );
};

/**
 * Append a new node at the begin of linked list
 */
extern struct node_t *node_insert ( struct node_t **head );

/**
 * Append a new node to the end of linked list
 */
extern struct node_t *node_append ( struct node_t **head );

/**
 * Concatenate paths together
 */
extern int path_concat ( char *path, size_t path_size, const char *name );

/**
 * Scan files tree for archive building
 */
extern int scan_files_tree ( const char *files[], size_t nfiles, struct node_t **root );

/**
 * Free node list from memory, also free node names if needed
 */
extern void free_files_tree ( struct node_t *root, int dynamic_names );

/**
 * Free node list from memory, do not free node names
 */
extern void free_node_list ( struct node_t *root );

/** 
 * Pack files to an archive
 */
extern int sbox_pack_archive ( const char *archive, uint32_t options, int level,
    const char *password, const char *files[], size_t nfiles );

/** 
 * Unpack files from an archive
 */
extern int sbox_unpack_archive ( const char *archive, uint32_t options, const char *password );

/**
 * Show operation progress with current file path
 */
extern void show_progress ( char action, const char *path );

/**
 * Open generic output stream
 */
extern void generic_ostream_open ( struct stream_base_context_t *context, struct io_stream *io );

/**
 * Open generic input stream
 */
extern void generic_istream_open ( struct stream_base_context_t *context, struct io_stream *io );

/** 
 * Generic put archive header
 */
extern int generic_put_header ( struct ar_ostream *stream, const struct header_t *header );

/** 
 * Generic get archive header
 */
extern int generic_get_header ( struct ar_istream *stream, struct header_t *header );

/**
 * Write data to output stream
 */
extern int generic_write ( struct ar_ostream *stream, const void *data, size_t len );

/**
 * Read data from input stream
 */
extern int generic_read ( struct ar_istream *stream, void *data, size_t len );

/**
 * Verify full content read from input stream
 */
extern int generic_verify ( struct ar_istream *stream );

/*
 * Finalize output stream
 */
extern int generic_flush ( struct ar_ostream *stream );

/*
 * Close stream
 */
extern void generic_close ( struct ar_stream *stream );

/**
 * Open plain output stream
 */
extern struct ar_ostream *plain_ostream_open ( struct io_stream *io );

/**
 * Open plain input stream
 */
extern struct ar_istream *plain_istream_open ( struct io_stream *io );

/**
 * Open lz4 output stream
 */
extern struct ar_ostream *lz4_ostream_open ( struct io_stream *io, int level );

/**
 * Open lz4 input stream
 */
extern struct ar_istream *lz4_istream_open ( struct io_stream *io );

/**
 * Calculate checksum of data
 */
extern uint32_t crc32b ( uint32_t crc, const uint8_t * buf, size_t len );

/**
 * Assign file descriptior with input IO stream
 */
extern struct io_stream *io_istream_from ( int fd, const char *password );

/**
 * Assign file descriptior with output IO stream
 */
extern struct io_stream *io_ostream_from ( int fd, const char *password );

/**
 * Assign file descriptior with IO stream
 */
extern struct io_stream *io_stream_new ( int fd );

/**
 * Read complete data chunk from file
 */
extern int read_complete ( int fd, uint8_t * mem, size_t total );

/**
 * Write complete data chunk to file
 */
extern int write_complete ( int fd, const uint8_t * mem, size_t total );

/**
 * Get random bytes
 */
extern int random_bytes ( void *buffer, size_t length );

#ifdef ENABLE_ENCRYPTION

/**
 * Assign file descriptior with IO AES input stream
 */
extern struct io_stream *io_aes_istream_new ( int fd, const char *password );

/**
 * Assign file descriptior with IO AES output stream
 */
extern struct io_stream *io_aes_ostream_new ( int fd, const char *password );

#endif
#endif
