/*
*  ===================================================================	
*	2023 - libimage by Ernesto Bayma
*  ===================================================================
*/

#ifndef __LIB_IMAGE_COMMON_H__
#define __LIB_IMAGE_COMMON_H__

#include <inttypes.h>
#include <stdio.h>

#define STATIC_ARRAY_SIZE(array) (int)((sizeof((array))/sizeof((array[0]))))
#define IS_POWER_OF_TWO(num)			(((int)(num)) & ((((int)num) - 1)))
#define Kilo(num) ((num) * 1024)
#define Mega(num) (Kilo((num)) * 1024)
#define Giga(num) (Mega((num)) * 1024)

typedef struct libimage_image_info {
	uint32_t width;
	uint32_t height;
	uint32_t gamma;
	uint8_t  color_type;

	uint8_t  *compressed_data;
	off_t	 cd_offset, cd_size;
	uint8_t  *uncompressed_data;
	off_t	 un_offset, un_size;
	uint8_t  *processed_data;
	off_t	 pr_offset, pr_size;

	int error;
} LibImageImageInfo;

typedef struct libimage_data_reader {
	uint8_t 	*data;
	uint16_t	type;
	uint16_t	error;
	uint32_t	cursor;
	uint32_t	peek_cursor;
} LibImageDataReader;

enum {
	LIBIMAGE_PNG_ERROR_HEADER = 1,
	LIBIMAGE_PNG_HUFFMAN_BAD_CODE_LENGTHS,
	LIBIMAGE_PNG_ERROR_IHDR_INTERLACE,
	LIBIMAGE_PNG_ERROR_BIG_IMAGE,
	LIBIMAGE_PNG_ERROR_IHDR_NOT_FOUND,
	LIBIMAGE_PNG_ERROR_INVALID_FILE,
	LIBIMAGE_PNG_ERROR_ZERO_SIZE,
	LIBIMAGE_PNG_ERROR_IDAT_SIZE_LIMIT,
	LIBIMAGE_PNG_ERROR_IHDR_BIT_DEPTH,
	LIBIMAGE_PNG_ERROR_CORRUPT_IHDR,
	LIBIMAGE_PNG_ERROR_IHDR_COLOUR_TYPE,
	LIBIMAGE_PNG_ERROR_IHDR_BIT_DEPTH_COMBINATION,
	LIBIMAGE_PNG_ERROR_CRC_NOT_MATCH,
	LIBIMAGE_PNG_ERROR_MULTIPLE_IHDR,
	LIBIMAGE_PNG_ERROR_NO_IDAT,
	LIBIMAGE_PNG_ERROR_NO_PLTE,
	LIBIMAGE_PNG_ERROR_GAMA_AFTER_PLTE,
	LIBIMAGE_PNG_ERROR_MULTIPLE_GAMA,
	LIBIMAGE_PNG_ERROR_UNEXPECTED_PLTE,
	LIBIMAGE_ERROR_TYPE_NOT_SUPPORTED,
	LIBIMAGE_ERROR_ZBUF_UNREACHABLE_STATE,
	LIBIMAGE_ERROR_INVALID_ZLIB_VALUE,
	LIBIMAGE_ERROR_OUT_OF_MEMORY,
	LIBIMAGE_PNG_ERROR_ZLIB_COMPRESSION,
	LIBIMAGE_ERROR_ZLIB_HEADER_CORRUPTED,
	LIBIMAGE_PNG_ERROR_PRESET_DICT,
	LIBIMAGE_PNG_ERROR_CORRUPTED_FILE,
	LIBIMAGE_ERROR_MEMORY_ERROR
};

enum {
	LIBIMAGE_TYPE_PNG = 1
};

uint32_t bit_reverse(uint32_t value, int bits);
uint32_t u32_endian_swap(uint32_t value);
int check_data_header(LibImageDataReader *r);

void consume_bytes(LibImageDataReader *r, int n);
uint8_t *read_from_reader(LibImageDataReader *r);
uint8_t *peek_from_reader(LibImageDataReader *r, int n);
void copy_to_buffer(uint8_t *dst, uint8_t *src, int size);
void libimage_printf(const char *fmt, ...);
#endif
