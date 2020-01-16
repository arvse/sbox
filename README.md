Licence: GPL

Usage:
```
usage: sbox -{cxelth}[snb0..9] archive [path]

version: 1.0.16

options:
  -c    create new archive
  -x    extract archive
  -e    extract archive, no paths
  -l    list only files in archive
  -t    check archive checksum
  -h    show help message
  -s    skip additional info
  -n    turn off lz4 compression
  -b    use best compression ratio
  -0..9 preset compression ratio
```

How to build?

Install mbedtls and lz4, then run make
