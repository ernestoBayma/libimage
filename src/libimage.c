/*
*  ===================================================================	
*	2023 - libimage by Ernesto Bayma
*  ===================================================================
*/

#include <stdio.h>
#include <stdlib.h>
#include <stdarg.h>
#include <assert.h>
#include <string.h>
#include <limits.h>
#include <inttypes.h>

#include "common.h"
#include "zlib.h"
#include "png.h"
#include "huffman.h"

#define LIBIMAGE_DEBUG 1
int check_data_header(LibImageDataReader *r)
{
	if(check_png_signature(r) == 0) {
		r->type = LIBIMAGE_TYPE_PNG;
	} else {
		r->error = LIBIMAGE_ERROR_TYPE_NOT_SUPPORTED;
		return -1;
	}
	return 0;
}

void libimage_error_code_to_msg(char *buffer, int buffer_size, int error)
{
char *msg;

	switch(error) {
	case LIBIMAGE_PNG_HUFFMAN_BAD_CODE_LENGTHS: msg = "Bad decoded huffman codelen. PNG file corrupted."; break;
	case LIBIMAGE_PNG_ERROR_HEADER: 	  msg = "Data has wrong file signature in the header for a PNG file."; break;
	case LIBIMAGE_PNG_ERROR_BIG_IMAGE:        msg = "Image dimensions are bigger than the maximum supported."; break;
	case LIBIMAGE_PNG_ERROR_ZERO_SIZE:        msg = "Dimensions of the image is zero. Corrupted PNG file."; break;
	case LIBIMAGE_PNG_ERROR_INVALID_FILE:     msg = "Data has invalid sequence for a PNG file."; break;
	case LIBIMAGE_PNG_ERROR_IHDR_NOT_FOUND:   msg = "Data don't start with the IHDR chunk which need to be the first chunk for a PNG file."; break;
	case LIBIMAGE_PNG_ERROR_IHDR_INTERLACE:   msg = "Data has a invalid value for interlace method on IHDR chunk."; break;
	case LIBIMAGE_PNG_ERROR_IHDR_BIT_DEPTH:   msg = "Data has a invalid value for the bit depth field on IHDR chunk."; break;	
	case LIBIMAGE_PNG_ERROR_CORRUPT_IHDR:	  msg = "IHDR chunk was invalid size."; break;
	case LIBIMAGE_PNG_ERROR_IHDR_COLOUR_TYPE: msg = "Data has a invalid value for the colour type field on IHDR chunk."; break;	
	case LIBIMAGE_PNG_ERROR_IHDR_BIT_DEPTH_COMBINATION: msg = "Data has a invalid combination between bit depth and colour type on IHDR chunk."; break;	
	case LIBIMAGE_PNG_ERROR_CRC_NOT_MATCH: 	 msg = "Data has a calculated crc that don't match the crc on the chunk."; break;
	case LIBIMAGE_PNG_ERROR_MULTIPLE_IHDR: 	 msg = "Data has multiple IHDR chunks. Which is not supported by the PNG spec."; break;
	case LIBIMAGE_PNG_ERROR_NO_IDAT: 	 msg = "Data has no IDAT chunk for a PNG file."; break;
	case LIBIMAGE_PNG_ERROR_NO_PLTE:  	 msg = "Expected a PLTE chunk based on Image type field from IHDR, but none was found."; break; 
	case LIBIMAGE_PNG_ERROR_GAMA_AFTER_PLTE: msg = "Got gAMA chunk after PLTE chunk."; break;
	case LIBIMAGE_PNG_ERROR_MULTIPLE_GAMA:   msg = "Got a another gAMA chunk, which is unsuported by PNG spec."; break; 
	case LIBIMAGE_PNG_ERROR_UNEXPECTED_PLTE: msg = "Got a PLTE but chunk Image type field from IHDR don't support it."; break; 
	case LIBIMAGE_PNG_ERROR_IDAT_SIZE_LIMIT: msg = "IDAT chunk size is bigger that the size limit. Corrupted PNG"; break;
	case LIBIMAGE_ERROR_MEMORY_ERROR:	 msg = "Error when manipulating memory."; break;
	case LIBIMAGE_ERROR_OUT_OF_MEMORY:	 msg = "Out of memory."; break;
	case LIBIMAGE_ERROR_TYPE_NOT_SUPPORTED:  msg = "Data has not supported header info."; break;
	case LIBIMAGE_ERROR_ZBUF_UNREACHABLE_STATE: msg = "Error trying to fill buffer."; break;
	case LIBIMAGE_ERROR_INVALID_ZLIB_VALUE: msg = "ZLib code is invalid."; break;
	case LIBIMAGE_PNG_ERROR_ZLIB_COMPRESSION: msg = "Compression method is no DEFLATE."; break;
	case LIBIMAGE_ERROR_ZLIB_HEADER_CORRUPTED: msg= "Zlib header is corrupted."; break;
	case LIBIMAGE_PNG_ERROR_CORRUPTED_FILE: msg = "PNG file is corrupted."; break;
	case LIBIMAGE_PNG_ERROR_PRESET_DICT: msg = "PNG spec don't allow preset dict on zlib header."; break;
	default: msg = "Unknown error. RUN."; break;
	}

	snprintf(buffer, buffer_size, "%s", msg);
}

void libimage_free_info_ptrs(LibImageImageInfo *info)
{
	if(info->uncompressed_data) {
		free(info->uncompressed_data);
		info->uncompressed_data = NULL;
	}
	if(info->processed_data) {
		free(info->processed_data);
		info->processed_data = NULL;
	}
	if(info->compressed_data) {
		free(info->compressed_data);
		info->compressed_data = NULL;
	}
}

void *libimage_process_data(uint8_t *data, uint32_t *width, unsigned int *height, int *error)
{
LibImageDataReader	reader;
LibImageImageInfo	info 	= {0};

	if(width)  *width  = 0;
	if(height) *height = 0;
	if(error)  *error  = 0;

	reader.data 		= data;
	reader.error		= 0;
	reader.cursor 		= 0;
	reader.peek_cursor	= 0;

	if(check_data_header(&reader)) {
		if(error) *error = reader.error;
		return NULL;
	}

	if(reader.type == LIBIMAGE_TYPE_PNG) libimage_process_png(&reader, &info);	
	else return NULL;

	if(reader.error) {
		if(error) *error = reader.error;
		libimage_free_info_ptrs(&info);	
	} else {
		if(width) 	*width = info.width;
		if(height)	*height = info.height;
	}

	return info.processed_data;
}
