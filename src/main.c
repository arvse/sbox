/* ------------------------------------------------------------------
 * SBox - Program Startup
 * ------------------------------------------------------------------ */

#include "sbox.h"

/**
 * Show program usage message
 */
static void show_usage ( void )
{
    fprintf ( stderr, "usage: sbox -{cxelthp}[snb0..9] [stdin|password] archive path [paths...]\n"
        "\n"
        "version: " SBOX_VERSION "\n"
        "\n"
        "options:\n"
        "  -c    create new archive\n"
        "  -x    extract archive\n"
        "  -l    list only files in archive\n"
        "  -t    test archive checksum\n"
        "  -h    show help message\n"
        "  -s    do not print progress\n"
        "  -n    turn off lz4 compression\n"
        "  -b    use best compression ratio\n"
        "  -p    use password protection\n" "  -0..9 preset compression ratio\n" "\n" );
}

/**
 * Check password strength
 */
static int check_password ( const char *password )
{
    char c;
    int has_upper_case = 0;
    int has_lower_case = 0;
    int has_digit = 0;
    int has_special = 0;
    size_t i;
    size_t len;

    len = strlen ( password );

    if ( len < 10 )
    {
        fprintf ( stderr, "Error: Password is too short.\n" );
        return 0;
    }

    for ( i = 0; i < len; i++ )
    {
        c = password[i];

        if ( isupper ( c ) )
        {
            has_upper_case = 1;

        } else if ( islower ( c ) )
        {
            has_lower_case = 1;

        } else if ( isdigit ( c ) )
        {
            has_digit = 1;

        } else
        {
            has_special = 1;
        }
    }

    if ( !has_upper_case )
    {
        fprintf ( stderr, "Warning: At least one upper case letter required.\n" );

    }

    if ( !has_lower_case )
    {
        fprintf ( stderr, "Warning: At least one lower case letter required.\n" );

    }

    if ( !has_digit )
    {
        fprintf ( stderr, "Warning: At least one digit required.\n" );

    }

    if ( !has_special )
    {
        fprintf ( stderr, "Warning: At least one special character required.\n" );
    }

    if ( !has_upper_case || !has_lower_case || !has_digit || !has_special )
    {
        return 0;
    }

    return 1;
}

/**
 * Parse compression level
 */
#ifndef EXTRACT_ONLY
static int parse_compression_level ( char *input )
{
    char c = '\0';

    if ( strlen ( input ) == 1 )
    {
        c = input[0];
    }

    switch ( c )
    {
    case '0':
        return 0;
    case '1':
        return 1;
    case '2':
        return 2;
    case '3':
        return 3;
    case '4':
        return 4;
    case '5':
        return 5;
    case '6':
        return 6;
    case '7':
        return 7;
    case '8':
        return 8;
    case '9':
    case 'b':
        return 9;
    default:
        return 6;
    }
}
#endif

/**
 * Check if flag is set in the options string
 */
static int check_flag ( const char *options, char flag )
{
    return !!strchr ( options, flag );
}

/**
 * Read password from stdin
 */
#ifdef ENABLE_STDIN_PASSWORD
#include <termios.h>
static int read_stdin_password ( char *password, size_t size )
{
    int c;
    size_t i;
    struct termios termios_backup;
    struct termios termios_current;

    if ( tcgetattr ( STDIN_FILENO, &termios_backup ) < 0 )
    {
        return -1;
    }

    termios_current = termios_backup;

    termios_current.c_lflag &= ~( ECHO );

    if ( tcsetattr ( STDIN_FILENO, TCSANOW, &termios_current ) < 0 )
    {
        return -1;
    }

    for ( i = 0; i + 1 < size; i++ )
    {
        c = getchar (  );

        if ( c == '\n' || c == EOF )
        {
            break;
        }

        password[i] = c;
    }

    password[i] = '\0';

    if ( tcsetattr ( STDIN_FILENO, TCSANOW, &termios_backup ) < 0 )
    {
        return -1;
    }

    return 0;
}
#endif

/**
 * Program entry point
 */
int main ( int argc, char *argv[] )
{
    int status = 0;
#ifndef EXTRACT_ONLY
    int level;
#endif
    uint32_t options = OPTION_VERBOSE | OPTION_LZ4;
    int arg_off;
    int flag_c;
    int flag_x;
    int flag_l;
    int flag_t;
    int flag_s;
    int flag_n;
    int flag_p;
    const char *password = NULL;
#ifdef ENABLE_STDIN_PASSWORD
    char password_buf[256];
#endif

    /* Validate arguments count */
    if ( argc < 3 )
    {
        show_usage (  );
        return 1;
    }

    /* Parse flags from arguments */
    flag_c = check_flag ( argv[1], 'c' );
    flag_x = check_flag ( argv[1], 'x' );
    flag_l = check_flag ( argv[1], 'l' );
    flag_t = check_flag ( argv[1], 't' );
    flag_s = check_flag ( argv[1], 's' );
    flag_n = check_flag ( argv[1], 'n' );
    flag_p = check_flag ( argv[1], 'p' );

    /* Get password from command line */
    arg_off = !!flag_p;

    /* Tasks are exclusive */
    if ( flag_c + flag_x + flag_l + flag_t != 1 )
    {
        show_usage (  );
        return 1;
    }

    /* Unset verbose if silent mode flag set */
    if ( flag_s )
    {
        options &= ~OPTION_VERBOSE;
    }

    /* Set list only option if needed */
    if ( flag_l )
    {
        options |= OPTION_LISTONLY;
    }

    /* Set test only option if needed */
    if ( flag_t )
    {
        options |= OPTION_TESTONLY;
    }

    /* Unset lz4 compression if needed */
    if ( flag_n )
    {
        options &= ~OPTION_LZ4;
    }
#ifndef EXTRACT_ONLY
    /* Parse compression level */
    level = parse_compression_level ( argv[1] );
#endif

    /* Get password from command line */
    if ( flag_p )
    {
        if ( argc < arg_off + 3 )
        {
            show_usage (  );
            return 1;
        }
        password = argv[2];

        if ( !strcmp ( password, "stdin" ) )
        {
#ifdef ENABLE_STDIN_PASSWORD
            if ( read_stdin_password ( password_buf, sizeof ( password_buf ) ) < 0 )
            {
                fprintf ( stderr, "Error: Failed to read stdin password.\n" );
                return 1;
            }

            password = password_buf;
#else
            fprintf ( stderr, "Error: Reading password from stdin not enabled.\n" );
#endif
        }

        /* Check password strength */
        if ( !check_password ( password ) )
        {
            fprintf ( stderr, "Error: Password is too weak.\n" );
            return 1;
        }
    }

    /* Perform the task */
    if ( flag_c )
    {
        if ( argc < arg_off + 4 )
        {
            show_usage (  );
            return 1;
        }
#ifdef EXTRACT_ONLY
        fprintf ( stderr, "Error: Archive creation not enabled.\n" );
        status = -1;
#else
        status =
            sbox_pack_archive ( argv[arg_off + 2], options, level, password,
            ( const char ** ) ( argv + arg_off + 3 ) );

#endif
    } else if ( flag_x || flag_l || flag_t )
    {
        if ( argc != arg_off + 3 )
        {
            show_usage (  );
            return 1;
        }
        status = sbox_unpack_archive ( argv[arg_off + 2], options, password );
    }

    /* Finally print error code and quit if found */
    if ( status < 0 )
    {
        fprintf ( stderr, "failure: %i\n", errno ? errno : -1 );
        return 1;
    }

    return 0;
}
