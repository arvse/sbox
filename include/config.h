/* ------------------------------------------------------------------
 * SBox - Simple Data Achive Utility
 * ------------------------------------------------------------------ */

#ifndef WIN32_BUILD
#include <arpa/inet.h>
#else
#include <windows.h>
#endif
#include <ctype.h>
#include <errno.h>
#include <fcntl.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>
#include <sys/stat.h>
#include <unistd.h>
#include <sys/types.h>
#include <dirent.h>
#include <stdlib.h>

#ifndef sbox_CONFIG_H
#define sbox_CONFIG_H

#ifndef ENODATA
#define ENODATA 61
#endif

#ifndef UNUSED
#define UNUSED(x) (void)(x)
#endif

#ifndef O_BINARY
#define O_BINARY 0
#endif

#define PATH_LIMIT 2048
#define WORKBUF_LIMIT 65536
#define CHUNK 65536

#endif
