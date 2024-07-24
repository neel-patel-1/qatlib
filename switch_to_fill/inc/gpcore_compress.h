#ifndef GPCORE_COMPRESS_H
#define GPCORE_COMPRESS_H

extern "C" {
  #include <zlib.h>
  #include <stdio.h>
  #include <stdint.h>
  #include <string.h>
}
#include "print_utils.h"

static int gpcore_do_decompress(void *dst, void *src, int src_len, int *out_len);


#include "inline/gpcore_compress.ipp"

#endif