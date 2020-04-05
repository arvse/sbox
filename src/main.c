/* ------------------------------------------------------------------
 * SBox - Simple Data Achive Utility
 * ------------------------------------------------------------------ */

#include "sbox.h"

/**
 * Show program usage message
 */
static void show_usage ( void )
{
    fprintf ( stderr, "usage: sbox -{cxelthp}[snb0..9] [stdin|password] archive [path]\n"
        "\n"
        "version: " sbox_VERSION "\n"
        "\n"
        "options:\n"
        "  -c    create new archive\n"
        "  -x    extract archive\n"
        "  -e    extract archive, no paths\n"
        "  -l    list only files in archive\n"
        "  -t    check archive checksum\n"
        "  -h    show help message\n"
        "  -s    skip additional info\n"
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
        fprintf ( stderr, "password is too short.\n" );
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
        fprintf ( stderr, "at least one upper case letter!\n" );
    } else if ( !has_lower_case )
    {
        fprintf ( stderr, "at least one lower case letter!\n" );
    } else if ( !has_digit )
    {
        fprintf ( stderr, "at least one digit!\n" );
    } else if ( !has_special )
    {
        fprintf ( stderr, "at least one special character!\n" );
    }

    if ( !has_upper_case || !has_lower_case || !has_digit || !has_special )
    {
        return 0;
    }

    return 1;
}

/** 
 * Check if flag is set
 */
static int check_flag ( const char *str, char flag )
{
    return strchr ( str, flag ) != NULL;
}

/**
 * Program entry point
 */
int main ( int argc, char *argv[] )
{
    int status = 0;
#ifndef EXTRACT_ONLY
    int level = 6;
#endif
    uint32_t options = OPTION_VERBOSE | OPTION_LZ4;
    int arg_off;
    int flag_c;
    int flag_x;
    int flag_e;
    int flag_l;
    int flag_t;
    int flag_s;
    int flag_n;
    int flag_p;
    ssize_t len;
    const char *password = NULL;
    char password_buf[256];

    /* Validate arguments count */
    if ( argc < 3 )
    {
        show_usage (  );
        return 1;
    }

    /* Parse flags from arguments */
    flag_c = check_flag ( argv[1], 'c' );
    flag_x = check_flag ( argv[1], 'x' );
    flag_e = check_flag ( argv[1], 'e' );
    flag_l = check_flag ( argv[1], 'l' );
    flag_t = check_flag ( argv[1], 't' );
    flag_s = check_flag ( argv[1], 's' );
    flag_n = check_flag ( argv[1], 'n' );
    flag_p = check_flag ( argv[1], 'p' );

    /* Get password from command line */
    arg_off = !!flag_p;

    /* Validate selected tasks count */
    if ( flag_c + flag_x + flag_e + flag_l + flag_t != 1 )
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

    /* Set no paths option if needed */
    if ( flag_e )
    {
        options |= OPTION_NOPATHS;
    }

    /* Set no paths option if needed */
    if ( flag_n )
    {
        options &= ~OPTION_LZ4;
    }
#ifndef EXTRACT_ONLY
    /* Adjust compression level */
    if ( strchr ( argv[1], '0' ) )
    {
        level = 0;

    } else if ( strchr ( argv[1], '1' ) )
    {
        level = 1;

    } else if ( strchr ( argv[1], '2' ) )
    {
        level = 2;

    } else if ( strchr ( argv[1], '3' ) )
    {
        level = 3;

    } else if ( strchr ( argv[1], '4' ) )
    {
        level = 4;

    } else if ( strchr ( argv[1], '5' ) )
    {
        level = 5;

    } else if ( strchr ( argv[1], '6' ) )
    {
        level = 6;

    } else if ( strchr ( argv[1], '7' ) )
    {
        level = 7;

    } else if ( strchr ( argv[1], '8' ) )
    {
        level = 8;

    } else if ( strchr ( argv[1], '9' ) || strchr ( argv[1], 'b' ) )
    {
        level = 9;
    }
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
        
        if (!strcmp(password, "stdin")) {
            printf("password?\n");
            if ((len = read(0, password_buf, sizeof(password_buf) -1)) <= 0) {
                fprintf(stderr, "failed to read password\n");
                return 1;
            }
            if (len > 0 && password_buf[len-1] == '\n') {
                len--;
            }
            password_buf[len] = '\0';
            password=password_buf;
        }

        if ( !check_password ( password ) )
        {
            fprintf ( stderr, "password is too weak :(\n" );
            return 1;
        }
    }

    /* Perform appriopriate action */
    if ( flag_c )
    {
        if ( argc < arg_off + 4 )
        {
            show_usage (  );
            return 1;
        }
#ifndef EXTRACT_ONLY
        status =
            sbox_pack_archive ( argv[arg_off + 2], options, level, password,
            ( const char ** ) ( argv + arg_off + 3 ), argc - arg_off - 3 );
#else

        fprintf ( stderr, "archive create not enabled.\n" );
        status = -1;
#endif

    } else if ( flag_x || flag_e || flag_l || flag_t )
    {
        if ( argc < arg_off + 3 )
        {
            show_usage (  );
            return 1;
        }
        status = sbox_unpack_archive ( argv[arg_off + 2], options, password );
    }

    /* Show failure message if needed */
    if ( status < 0 )
    {
        fprintf ( stderr, "failure: %i\n", errno ? errno : -1 );
    }

    return status < 0;
}
