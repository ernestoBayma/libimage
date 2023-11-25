#ifndef __LIB_IMAGE_ZLIB_H__
#define __LIB_IMAGE_ZLIB_H__

#include <inttypes.h>
#include "common.h"

typedef struct libimage_zlib_header {
	uint32_t compression_method_and_info;
	uint32_t extra_flags;
} LibImageZlibHeader;

typedef struct libimage_zlib_buf {
	uint8_t *buf, *buf_end;
	uint8_t *sliding_window, *sliding_window_cur_pos;
	off_t	 sliding_window_off, sliding_window_limit;	
	uint32_t code_buf, code_buf_bits;
	int	 error, allocated_window;
} LibImageZlibBuffer;

void zbuf_init(LibImageZlibBuffer *buf, uint8_t *contents, uint32_t content_len, int alloc_window);
int zbuf_append_to_sliding_window(LibImageZlibBuffer *buf, uint8_t *value, int size_value);
void zbuf_deinit(LibImageZlibBuffer *buf);
int zbuf_is_eof(LibImageZlibBuffer *buf);
uint8_t zbuf_get_byte(LibImageZlibBuffer *buf);
void zbuf_fill_code_buf(LibImageZlibBuffer *buf);
uint32_t zbuf_get_n_bits(LibImageZlibBuffer *buf, int n);
void zbuf_parse_header(LibImageZlibBuffer *buf);
void write_uncompressed_data(LibImageZlibBuffer *buf, LibImageImageInfo *info, int size, char *opt_value);
#endif
