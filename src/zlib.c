/*
*  ===================================================================	
*	2023 - libimage by Ernesto Bayma
*  ===================================================================
*/
#include <stdlib.h>
#include <string.h>
#include <inttypes.h>

#include "zlib.h"
#include "common.h"

void zbuf_init(LibImageZlibBuffer *buf, uint8_t *contents, uint32_t content_len, int alloc_window)
{
int i;

	buf->buf = contents;
	buf->buf_end = contents + content_len;
	buf->sliding_window_cur_pos = buf->sliding_window = NULL;
	buf->sliding_window_off = 0;
	buf->sliding_window_limit = 0;
	buf->error = 0;
	buf->allocated_window = 0;
	buf->code_buf = 0;
	buf->code_buf_bits = 0;
	if(alloc_window	> 0) {
		buf->sliding_window = malloc(sizeof(uint8_t) * Kilo(32) + 256);
		if(buf->sliding_window == NULL) {
			buf->error = LIBIMAGE_ERROR_OUT_OF_MEMORY;
			return;
		}
		memset(buf->sliding_window, 0, Kilo(32));
		for(i = 0; i < 256; i++) buf->sliding_window[i + Kilo(32)] = 0xff;
		buf->sliding_window_cur_pos = buf->sliding_window;
		buf->sliding_window_limit = Kilo(32);
		buf->allocated_window = 1;
	}
}

int zbuf_append_to_sliding_window(LibImageZlibBuffer *buf, uint8_t *value, int size_value)
{
	if(size_value + buf->sliding_window_off > buf->sliding_window_limit) return -1;
	memcpy(buf->sliding_window + buf->sliding_window_off, value, size_value);
	buf->sliding_window_off += size_value;
	buf->sliding_window_cur_pos = buf->sliding_window + buf->sliding_window_off;
	return 0;
}
void zbuf_deinit(LibImageZlibBuffer *buf)
{
	if(buf->allocated_window == 1) {
		free(buf->sliding_window);
		buf->sliding_window = NULL;
	}
}

int zbuf_is_eof(LibImageZlibBuffer *buf)
{
	return buf->buf >= buf->buf_end;
}

uint8_t zbuf_get_byte(LibImageZlibBuffer *buf)
{
	return zbuf_is_eof(buf) ? 0 : *buf->buf++;
}

void zbuf_fill_code_buf(LibImageZlibBuffer *buf)
{
	do {
		if(buf->code_buf >= (1 << buf->code_buf_bits)) {
			buf->buf = buf->buf_end;
			return;
		}
		buf->code_buf |= (uint32_t) zbuf_get_byte(buf) << buf->code_buf_bits;
		buf->code_buf_bits += 8;

	} while(buf->code_buf_bits <= 24);	
}

uint32_t zbuf_get_n_bits(LibImageZlibBuffer *buf, int n)
{
uint32_t code;
	if(buf->code_buf_bits < n) zbuf_fill_code_buf(buf);
	code = buf->code_buf & ((1 << n) - 1);
	buf->code_buf >>= n;
	buf->code_buf_bits -= n;
	return code;
}

void zbuf_parse_header(LibImageZlibBuffer *buf)
{
int	cmf, cm,flags;
	
	cmf = zbuf_get_byte(buf);
	flags = zbuf_get_byte(buf);
	cm = cmf & 15;
	if( zbuf_is_eof(buf) || (((cmf*256+flags) % 31) != 0)) {
		buf->error = LIBIMAGE_ERROR_ZLIB_HEADER_CORRUPTED;
		return;	
	}
	if(flags & 32) {
		buf->error =  LIBIMAGE_PNG_ERROR_PRESET_DICT;
		return;
	}
	if(cm != 8) {
		buf->error = LIBIMAGE_PNG_ERROR_ZLIB_COMPRESSION;
		return;
	}
}
void write_uncompressed_data(LibImageZlibBuffer *buf, LibImageImageInfo *info, int size, char *opt_value)
{
uint8_t *tmp, *src_used;
uint32_t mem_size;

	if(info->uncompressed_data == NULL) {
		mem_size = size > 1024 ? size : 1024;
		tmp = realloc(info->uncompressed_data, sizeof(char) * mem_size);
		if(tmp == NULL) {
			info->error = LIBIMAGE_ERROR_OUT_OF_MEMORY;
			return;
		}
		info->uncompressed_data = tmp;
		info->un_size = mem_size;
		info->un_offset = 0;
	} else {
		mem_size = info->un_size;
		while((size + info->un_offset) > mem_size) mem_size *= 2;
		if(mem_size != info->un_size) {
			tmp = realloc(info->uncompressed_data, sizeof(char) * mem_size);
			if(tmp == NULL) {
				info->error = LIBIMAGE_ERROR_OUT_OF_MEMORY;
				return;
			}
			info->uncompressed_data = tmp;
			info->un_size = mem_size;
		}
	}
	if(opt_value != NULL) src_used = opt_value;
	else src_used = buf->buf;

	copy_to_buffer(info->uncompressed_data + info->un_offset, src_used, size);
	src_used += size;
	info->un_offset += size;
}
