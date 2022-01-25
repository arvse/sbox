/* ------------------------------------------------------------------
 * SBox - Misc Utility Stuff
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
 * SBox archive prefix
 */
const uint8_t sbox_archive_prefix[ARCHIVE_PREFIX_LENGTH] = { 's', 'b', 'o', 'x' };
