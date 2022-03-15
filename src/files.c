/* ------------------------------------------------------------------
 * SBox - File Net Processing
 * ------------------------------------------------------------------ */

#include "sbox.h"

/**
 * Name chunk structure
 */
struct name_chunk_t
{
    size_t len;
    struct name_chunk_t *next;
    struct name_chunk_t *prev;
};

/**
 * Name stack structure
 */
struct name_stack_t
{
    size_t path_len;
    size_t path_size;
    char *path;
    struct name_chunk_t *head;
    struct name_chunk_t *tail;
};

/**
 * Expandable buffer structure
 */
struct ext_buffer_t
{
    uint8_t *bytes;
    size_t length;
    size_t capacity;
};

/**
 * Create new name stack
 */
static int name_stack_new ( struct name_stack_t *stack )
{
    stack->path_len = 0;
    stack->path_size = 256;

    if ( !( stack->path = ( char * ) malloc ( stack->path_size ) ) )
    {
        return -1;
    }

    stack->path[0] = '\0';

    stack->head = NULL;
    stack->tail = NULL;

    return 0;
}

/**
 * Push name into name stack
 */
static int name_stack_push ( struct name_stack_t *stack, const char *name )
{
    size_t name_len;
    size_t new_path_len;
    char *backup;
    struct name_chunk_t *chunk;

    name_len = strlen ( name );

    new_path_len = stack->path_len + !!stack->path[0] + name_len;

    if ( new_path_len >= stack->path_size )
    {
        stack->path_size = 2 * new_path_len;
        backup = stack->path;

        if ( !( stack->path = realloc ( stack->path, stack->path_size ) ) )
        {
            free ( backup );
            return -1;
        }
    }

    if ( stack->path[0] )
    {
        stack->path[stack->path_len++] = '/';
    }

    memcpy ( stack->path + stack->path_len, name, name_len + 1 );
    stack->path_len = new_path_len;

    if ( !( chunk = ( struct name_chunk_t * ) malloc ( sizeof ( struct name_chunk_t ) ) ) )
    {
        return -1;
    }

    chunk->len = name_len;

    chunk->next = NULL;
    chunk->prev = stack->tail;

    if ( stack->tail )
    {
        stack->tail->next = chunk;
    }

    stack->tail = chunk;

    if ( !stack->head )
    {
        stack->head = chunk;
    }

    return 0;
}

/**
 * Pop name from name stack and discard it
 */
static int name_stack_pop_discard ( struct name_stack_t *stack )
{
    struct name_chunk_t *last;

    if ( !stack->tail )
    {
        return -1;
    }

    last = stack->tail;

    if ( stack->path_len == last->len )
    {
        stack->path_len = 0;

    } else
    {
        if ( stack->path_len < 1 + last->len )
        {
            return -1;
        }

        stack->path_len -= 1 + last->len;
    }

    stack->path[stack->path_len] = '\0';

    stack->tail = last->prev;

    if ( stack->tail )
    {
        stack->tail->next = NULL;

    } else
    {
        stack->head = NULL;
    }

    free ( last );

    return 0;
}

/**
 * Free name stack from memory
 */
static void name_stack_free ( struct name_stack_t *stack )
{
    struct name_chunk_t *ptr;
    struct name_chunk_t *next;

    for ( ptr = stack->head; ptr; ptr = next )
    {
        next = ptr->next;
        free ( ptr );
    }

    free ( stack->path );
}

/**
 * Create new sbox node with name
 */
static struct sbox_node_t *sbox_node_new ( const char *name )
{
    size_t name_len;
    struct sbox_node_t *node;

    if ( !( node = ( struct sbox_node_t * ) calloc ( 1, sizeof ( struct sbox_node_t ) ) ) )
    {
        return NULL;
    }

    if ( name )
    {
        name_len = strlen ( name );

        if ( !( node->name = ( char * ) malloc ( name_len + 1 ) ) )
        {
            free ( node );
            return NULL;
        }

        memcpy ( node->name, name, name_len + 1 );
    }

    return node;
}

static int file_net_append_child ( struct sbox_node_t *parent, struct sbox_node_t *child )
{
    child->next = NULL;
    child->prev = parent->tail;

    if ( parent->tail )
    {
        parent->tail->next = child;
    }

    parent->tail = child;

    if ( !parent->head )
    {
        parent->head = child;
    }

    return 0;
}

/**
 * Get file basename from path
 */
static const char *file_net_get_basename ( const char *path )
{
    const char *backup;
    const char *ptr;

    ptr = path;

    do
    {
        backup = ptr;
        ptr = strchr ( ptr, '/' );
        if ( ptr )
        {
            ptr++;
        }
    } while ( ptr );

    return backup;
}

/**
 * Create new file net from paths internal
 */
struct sbox_node_t *build_file_net_in ( struct name_stack_t *stack, const char *name )
{
    DIR *dir;
    struct dirent *entry;
    struct sbox_node_t *node;
    struct sbox_node_t *child;
    struct stat statbuf;

    if ( !( node = sbox_node_new ( name ) ) )
    {
        return NULL;
    }

    if ( name_stack_push ( stack, name ) < 0 )
    {
        free_file_net ( node );
        return NULL;
    }

    if ( stat ( stack->path, &statbuf ) < 0 )
    {
        perror ( stack->path );
        free_file_net ( node );
        return NULL;
    }

    node->mode = statbuf.st_mode;
    node->mtime = statbuf.st_mtime;

    if ( statbuf.st_mode & S_IFDIR )
    {
        if ( !( dir = opendir ( stack->path ) ) )
        {
            perror ( stack->path );
            free_file_net ( node );
            return NULL;
        }

        while ( ( entry = readdir ( dir ) ) )
        {
            if ( !strcmp ( entry->d_name, "." ) || !strcmp ( entry->d_name, ".." ) )
            {
                continue;
            }

            if ( !( child = build_file_net_in ( stack, entry->d_name ) ) )
            {
                free_file_net ( node );
                return NULL;
            }

            file_net_append_child ( node, child );
        }

        closedir ( dir );

    } else
    {
        node->size = statbuf.st_size;
    }

    if ( name_stack_pop_discard ( stack ) < 0 )
    {
        free_file_net ( node );
        return NULL;
    }

    return node;
}

/**
 * Create new file net from paths
 */
struct sbox_node_t *build_file_net ( const char *paths[] )
{
    struct sbox_node_t *root;
    struct sbox_node_t *child;
    struct name_stack_t stack;

    if ( !( root = sbox_node_new ( NULL ) ) )
    {
        return NULL;
    }

    if ( name_stack_new ( &stack ) < 0 )
    {
        free_file_net ( root );
        return NULL;
    }

    if ( !paths[0] )
    {
        free_file_net ( root );
        return NULL;
    }

    while ( paths[0] )
    {
        if ( !( child = build_file_net_in ( &stack, *paths ) ) )
        {
            free_file_net ( root );
            name_stack_free ( &stack );
            return NULL;
        }

        file_net_append_child ( root, child );

        paths++;
    }

    if ( stack.head || stack.tail )
    {
        free_file_net ( root );
        root = NULL;
    }

    name_stack_free ( &stack );

    return root;
}

/**
 * Browse file net internal
 */
static int file_net_iter_in ( struct sbox_node_t *node, struct name_stack_t *stack, void *context,
    file_net_iter_callback callback )
{
    struct sbox_node_t *ptr;

    if ( name_stack_push ( stack, node->name ) < 0 )
    {
        return -1;
    }

    if ( callback ( context, node, stack->path ) < 0 )
    {
        return -1;
    }

    for ( ptr = node->head; ptr; ptr = ptr->next )
    {
        if ( file_net_iter_in ( ptr, stack, context, callback ) < 0 )
        {
            return -1;
        }
    }

    if ( name_stack_pop_discard ( stack ) < 0 )
    {
        return -1;
    }

    return 0;
}

/**
 * Browse file net
 */
int file_net_iter ( struct sbox_node_t *root, void *context, file_net_iter_callback callback )
{
    struct sbox_node_t *ptr;
    struct name_stack_t stack;

    if ( name_stack_new ( &stack ) < 0 )
    {
        return -1;
    }

    for ( ptr = root->head; ptr; ptr = ptr->next )
    {
        if ( file_net_iter_in ( ptr, &stack, context, callback ) < 0 )
        {
            name_stack_free ( &stack );
            return -1;
        }
    }

    if ( stack.head || stack.tail )
    {
        name_stack_free ( &stack );
        return -1;
    }

    name_stack_free ( &stack );
    return 0;
}

/**
 * Free file net from memory
 */
void free_file_net ( struct sbox_node_t *node )
{
    struct sbox_node_t *ptr;
    struct sbox_node_t *next;

    if ( node->name )
    {
        free ( node->name );
    }

    for ( ptr = node->head; ptr; ptr = next )
    {
        next = ptr->next;
        free ( ptr );
    }

    free ( node );
}

/**
 * Save file net to stream internal
 */
static int file_net_save_in ( struct sbox_node_t *node, struct io_stream_t *io )
{
    uint8_t type;
    uint8_t opcode;
    uint32_t net_mode;
    uint32_t net_size;
    const char *basename;
    struct sbox_node_t *ptr;

    if ( node->mode & S_IFDIR )
    {
        type = node->head ? 'd' : 'e';

    } else
    {
        type = 'f';
    }

    opcode = node->next ? toupper ( type ) : type;

    if ( io->write_complete ( io, &opcode, sizeof ( opcode ) ) < 0 )
    {
        return -1;
    }

    net_mode = htonl ( node->mode );

    if ( io->write_complete ( io, &net_mode, sizeof ( net_mode ) ) < 0 )
    {
        return -1;
    }

    if ( type == 'f' )
    {
        net_size = htonl ( node->size );

        if ( io->write_complete ( io, &net_size, sizeof ( net_size ) ) < 0 )
        {
            return -1;
        }
    }

    basename = file_net_get_basename ( node->name );

    if ( !*basename || strchr ( basename, '/' ) || !strcmp ( basename, ".." ) )
    {
        basename = ".";
    }

    if ( io->write_complete ( io, basename, strlen ( basename ) + 1 ) < 0 )
    {
        return -1;
    }

    for ( ptr = node->head; ptr; ptr = ptr->next )
    {
        if ( file_net_save_in ( ptr, io ) < 0 )
        {
            return -1;
        }
    }

    return 0;
}

/**
 * Save file net to stream
 */
int file_net_save ( struct sbox_node_t *root, struct io_stream_t *io )
{
    struct sbox_node_t *ptr;

    for ( ptr = root->head; ptr; ptr = ptr->next )
    {
        if ( file_net_save_in ( ptr, io ) < 0 )
        {
            return -1;
        }
    }

    return 0;
}

/**
 * Create new expandable buffer
 */
static int ext_buffer_new ( struct ext_buffer_t *buffer )
{
    buffer->length = 0;
    buffer->capacity = 256;

    if ( !( buffer->bytes = ( uint8_t * ) malloc ( buffer->capacity ) ) )
    {
        return -1;
    }

    return 0;
}

/**
 * Clear expandable buffer content
 */
static void ext_buffer_clear ( struct ext_buffer_t *buffer )
{
    buffer->length = 0;
}

/**
 * Append one byte to expandable buffer
 */
static int ext_buffer_append ( struct ext_buffer_t *buffer, uint8_t byte )
{
    uint8_t *backup;

    if ( buffer->length + 1 >= buffer->capacity )
    {
        buffer->capacity = 2 * ( buffer->length + 1 );
        backup = buffer->bytes;

        if ( !( buffer->bytes = realloc ( buffer->bytes, buffer->capacity ) ) )
        {
            free ( backup );
            return -1;
        }
    }

    buffer->bytes[buffer->length++] = byte;
    return 0;
}

/**
 * Free expandable buffer from memory
 */
static void ext_buffer_free ( struct ext_buffer_t *buffer )
{
    free ( buffer->bytes );
}

/**
 * Load file net from stream internal
 */
struct sbox_node_t *file_net_load_in ( struct io_stream_t *io, struct ext_buffer_t *buffer,
    int *has_sibling )
{
    int type;
    int has_next = 1;
    uint8_t byte;
    uint32_t net_mode;
    uint32_t net_size;
    char *name;
    struct sbox_node_t *node;
    struct sbox_node_t *child;

    if ( io->read_complete ( io, &byte, sizeof ( byte ) ) < 0 )
    {
        return NULL;
    }

    *has_sibling = byte == toupper ( byte );

    type = tolower ( byte );

    if ( io->read_complete ( io, &net_mode, sizeof ( net_mode ) ) < 0 )
    {
        return NULL;
    }

    if ( type == 'f' )
    {
        if ( io->read_complete ( io, &net_size, sizeof ( net_size ) ) < 0 )
        {
            return NULL;
        }
    }

    ext_buffer_clear ( buffer );

    do
    {
        if ( io->read_complete ( io, &byte, sizeof ( byte ) ) < 0 )
        {
            return NULL;
        }

        if ( ext_buffer_append ( buffer, byte ) < 0 )
        {
            return NULL;
        }

    } while ( byte );

    name = ( char * ) buffer->bytes;

    if ( !strcmp ( name, ".." ) || strchr ( name, '/' ) )
    {
        node = sbox_node_new ( "_name_restricted_" );

    } else
    {
        node = sbox_node_new ( name );
    }

    if ( !node )
    {
        return NULL;
    }

    node->mode = ntohl ( net_mode );

    if ( type == 'f' )
    {
        node->size = ntohl ( net_size );
    }

    if ( type == 'd' )
    {
        while ( has_next )
        {
            if ( !( child = file_net_load_in ( io, buffer, &has_next ) ) )
            {
                free_file_net ( node );
                return NULL;
            }

            file_net_append_child ( node, child );
        }
    }

    return node;
}

/**
 * Load file net from stream
 */
struct sbox_node_t *file_net_load ( struct io_stream_t *io )
{
    int has_sibling = 1;
    struct sbox_node_t *root;
    struct sbox_node_t *child;
    struct ext_buffer_t buffer;

    if ( !( root = sbox_node_new ( NULL ) ) )
    {
        return NULL;
    }

    if ( ext_buffer_new ( &buffer ) < 0 )
    {
        free_file_net ( root );
        return NULL;
    }

    while ( has_sibling )
    {
        if ( !( child = file_net_load_in ( io, &buffer, &has_sibling ) ) )
        {
            ext_buffer_free ( &buffer );
            free_file_net ( root );
            return NULL;
        }

        file_net_append_child ( root, child );
    }

    ext_buffer_free ( &buffer );

    return root;
}
