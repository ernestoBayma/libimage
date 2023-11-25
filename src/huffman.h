/*
*  ===================================================================	
*	2023 - libimage by Ernesto Bayma
*  ===================================================================
*/
#ifndef __LIB_IMAGE_HUFFMAN_H__
#define __LIB_IMAGE_HUFFMAN_H__

#include <inttypes.h>
#include "common.h"
#include "zlib.h"

typedef struct libimage_huffman_entry {
	uint16_t bits_used;
	uint16_t code;
} LibImageHuffmanEntry;

typedef struct libimage_huffman {
	LibImageHuffmanEntry 	*entries;
	uint32_t		entry_count;
	uint32_t		max_code_in_bits;
} LibImageHuffman;

LibImageHuffman new_huffman(int max_code_in_bits);
LibImageHuffman free_huffman_entries(LibImageHuffman *huff);
void build_huffman(LibImageZlibBuffer *buf, LibImageHuffman *huff, uint8_t *code_len_bits, int size_code_len_bits);
uint32_t decode_huffman(LibImageHuffman *huff, LibImageZlibBuffer *buf);
void decompress_huffman_block(LibImageZlibBuffer *buf, LibImageImageInfo *info, int size_lit, int size_dist);

// This should go to png
void png_parse_huffman_fixed_block(LibImageZlibBuffer *buf, LibImageImageInfo *info);
void png_parse_huffman_dynamic_block(LibImageZlibBuffer *buf, LibImageImageInfo *info);

#endif
