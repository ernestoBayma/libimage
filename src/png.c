/*
*  ===================================================================	
*	2023 - libimage by Ernesto Bayma
*  ===================================================================
*/


#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <inttypes.h>
#include <limits.h>

#include "common.h"
#include "zlib.h"
#include "png.h"
#include "huffman.h"

#define IDAT_DEFAULT_BLOCK_SIZE		4096
#define MAXIMUM_LOOP_ALLOWED		UINT64_MAX - 1
#define LIBIMAGE_DEFLATE_COMPRESSION 0

/**
*    CRC algorithm
*
*    Chunk CRCs are calculated using standard CRC methods with pre and post conditioning, as defined by ISO 3309 [ISO-3309] or ITU-T V.42 [ITU-T-V42]. 
*    The CRC polynomial employed is x^32+x^26+x^23+x^22+x^16+x^12+x^11+x^10+x^8+x^7+x^5+x^4+x^2+x+1
*
*    The 32-bit CRC register is initialized to all 1's, and then the data from each byte is processed from the least significant bit (1) to the most significant bit (128). 
*    After all the data bytes are processed, the CRC register is inverted (its ones complement is taken). 
*    This value is transmitted (stored in the file) MSB first. For the purpose of separating into bytes and ordering, the least significant bit of the 32-bit CRC is defined 
*    to be the coefficient of the x31 term. 
**/

#define CRC_POLYNOMIAL 		0xebd88320
#define MAX_CRC_TABLE_VALUE	256
#define ALL_32_BITS_SET		0xffffffffL

/* Table of CRCs of all 8-bit messages. */
static uint32_t crc_table[256];
/* Flag: has the table been computed? Initially false. */
static int crc_table_computed = 0;
/* Make the table for a fast CRC. */
void make_crc_table(void)
{
uint32_t 	c;
int 		n, k;

	for (n = 0; n < MAX_CRC_TABLE_VALUE; n++) {
		c = (uint32_t) n;
		for (k = 0; k < 8; k++) {
			c = c & 1 ? (c >> 1) ^ CRC_POLYNOMIAL : c >> 1;
		}
		crc_table[n] = c;
	}
	crc_table_computed = 1;
}
   
/* 
* 	Based on the Annex D code of the PNG Spec And zlib
*/
uint32_t crc(uint8_t *buf, int len)
{
uint32_t result;
int	 n;
	if (!crc_table_computed)
		make_crc_table();

	result = ALL_32_BITS_SET;
	for(int n = 0 ; n < len; n++) {
		result = crc_table[(result ^ buf[n]) & 0xff] ^ (result >> 8);	
	}

	result = (result ^ ALL_32_BITS_SET);
	result = u32_endian_swap(result);
	return result;
}

static uint8_t png_file_sig[]   = { 0x89, 0x50, 0x4e, 0x47, 0x0d, 0x0a, 0x1a, 0x0a };
static LibImageHuffmanEntry png_length_from_spec[] =
{
    {3, 0}, //  257
    {4, 0}, //  258
    {5, 0}, //  259
    {6, 0}, //  260
    {7, 0}, //  261
    {8, 0}, //  262
    {9, 0}, //  263
    {10, 0}, //  264
    {11, 1}, //  265
    {13, 1}, //  266
    {15, 1}, //  267
    {17, 1}, //  268
    {19, 2}, //  269
    {23, 2}, //  270
    {27, 2}, //  271
    {31, 2}, //  272
    {35, 3}, //  273
    {43, 3}, //  274
    {51, 3}, //  275
    {59, 3}, //  276
    {67, 4}, //  277
    {83, 4}, //  278
    {99, 4}, //  279
    {115, 4}, //  280
    {131, 5}, //  281
    {163, 5}, //  282
    {195, 5}, //  283
    {227, 5}, //  284
    {258, 0}, //  285
};
static LibImageHuffmanEntry png_dist_from_spec[] =
{
    {1, 0}, //  0
    {2, 0}, //  1
    {3, 0}, //  2
    {4, 0}, //  3
    {5, 1}, //  4
    {7, 1}, //  5
    {9, 2}, //  6
    {13, 2}, //  7
    {17, 3}, //  8
    {25, 3}, //  9
    {33, 4}, //  10
    {49, 4}, //  11
    {65, 5}, //  12
    {97, 5}, //  13
    {129, 6}, //  14
    {193, 6}, //  15
    {257, 7}, //  16
    {385, 7}, //  17
    {513, 8}, //  18
    {769, 8}, //  19
    {1025, 9}, //  20
    {1537, 9}, //  21
    {2049, 10}, //  22
    {3073, 10}, //  23
    {4097, 11}, //  24
    {6145, 11}, //  25
    {8193, 12}, //  26
    {12289, 12}, //  27
    {16385, 13}, //  28
    {24577, 13}, //  29
};

LibImageHuffman new_huffman(int max_code_in_bits)
{
LibImageHuffman huff;
	
	huff.max_code_in_bits = max_code_in_bits;
	huff.entry_count = 1 << max_code_in_bits;
	huff.entries = malloc(sizeof(struct libimage_huffman_entry) * huff.entry_count);

	return huff;
}

LibImageHuffman free_huffman_entries(LibImageHuffman *huff)
{
	if(huff->entries) free(huff->entries);
}

void decompress_huffman_block(LibImageZlibBuffer *buf, LibImageImageInfo *info, int size_lit, int size_dist)
{
LibImageHuffman 	lit_huff, dist_huff;
uint32_t		lit_len, extra_bits, distance, encoded_len, actual_len;
uint8_t			decompressed, *source;
LibImageHuffmanEntry	len_from_spec, dist_from_spec;

	lit_huff 	= new_huffman(15);
	dist_huff	= new_huffman(15);
	build_huffman(buf, &lit_huff, buf->sliding_window, size_lit);
	build_huffman(buf, &dist_huff, buf->sliding_window + size_lit, size_dist);

	while(1) {
		lit_len = decode_huffman(&lit_huff, buf);
		if(lit_len <= 255) {
			decompressed = (lit_len & 0xFF);
			write_uncompressed_data(buf, info, 1, &decompressed);
		} else if(lit_len >= 257) {
			encoded_len = (lit_len - 257);
			len_from_spec = png_length_from_spec[encoded_len];
			actual_len = len_from_spec.code;
			if(len_from_spec.bits_used > 0) {
				extra_bits = zbuf_get_n_bits(buf, len_from_spec.bits_used);
				actual_len += extra_bits;
			}

			encoded_len = decode_huffman(&dist_huff, buf);
			dist_from_spec = png_dist_from_spec[encoded_len];
			
			distance = dist_from_spec.code;
			if(dist_from_spec.bits_used > 0) {
				extra_bits = zbuf_get_n_bits(buf, dist_from_spec.bits_used);
				distance += extra_bits;
			}

			source = (info->uncompressed_data + info->un_offset) - distance;
			while(actual_len--) {
				write_uncompressed_data(buf, info, 1, source);
			}
		} else {
			break;
		}
	}

	free_huffman_entries(&dist_huff);
	free_huffman_entries(&lit_huff);
}

void build_huffman(LibImageZlibBuffer *buf, LibImageHuffman *huff, uint8_t *code_len_bits, int size_code_len_bits)
{
int      		i, next_code[19], c_len_bits, code_len_hist[19];
uint32_t 		code, shift_bits, symbol, big_endian_index, entry_index, actual_index, entry_count;
LibImageHuffmanEntry 	*entry;

	memset(next_code, 0, sizeof(int) * 19);
	memset(code_len_hist, 0, sizeof(int) * 19);

	for(i = 0; i < size_code_len_bits; i++) {
		code_len_hist[code_len_bits[i]]++;
	}

	code_len_hist[0] = 0;
	for(i = 1; i < STATIC_ARRAY_SIZE(next_code); i++) {
		next_code[i] = next_code[i - 1] + (code_len_hist[i - 1] << 1);
	}

	for(i = 0; i < size_code_len_bits; i++) {
		c_len_bits = code_len_bits[i];
		if(c_len_bits > 0) {
			code = next_code[c_len_bits]++;
			shift_bits  = (huff->max_code_in_bits - c_len_bits);
			entry_count = 1 << shift_bits;
			for(entry_index = 0; entry_index < entry_count; entry_index++) {
				big_endian_index = (code << shift_bits) | entry_index;
				actual_index = bit_reverse(big_endian_index, huff->max_code_in_bits);
				entry = huff->entries + actual_index;
				symbol = i;
				entry->bits_used = c_len_bits;
				entry->code    = symbol;
			}
		}
	}
}

uint32_t decode_huffman(LibImageHuffman *huff, LibImageZlibBuffer *buf)
{
uint32_t entry_index;

	entry_index = zbuf_get_n_bits(buf, huff->max_code_in_bits);
	return huff->entries[entry_index].code;
}

void print_ihdr(LibImagePngIHdr *h)
{
	printf("Width  %x\n", 		h->width);
	printf("Height %x\n", 		h->height);
	printf("Bit depth %x\n", 	h->bit_depth);
	printf("Color type %x\n", 	h->colour_type);
	printf("Compression method %x\n", h->compression_method);
	printf("Filter method %x\n", h->filter_method);
	printf("Interlace method %x\n", h->interlace_method);
}

int check_png_signature(LibImageDataReader *r)
{
uint64_t sig_to_check, correct_sig;
	correct_sig 	= *(uint64_t*)png_file_sig;
	sig_to_check	= *(uint64_t*)r->data;	

	if(sig_to_check != sig_to_check) return -1;

	consume_bytes(r, sizeof(correct_sig));
	return 0;
}

int validate_ihdr(LibImagePngIHdr *h, LibImageImageInfo *i)
{
	if(h == NULL) return 0;
	
	if(h->colour_type < PNG_COLOR_TYPE_GREYSCALE && h->colour_type > PNG_COLOR_TYPE_TRUE_COLOUR_WITH_ALPHA) return LIBIMAGE_PNG_ERROR_IHDR_COLOUR_TYPE;
	if(h->colour_type == 1 || h->colour_type == 5) return LIBIMAGE_PNG_ERROR_IHDR_COLOUR_TYPE;

	if(h->bit_depth > 16 && h->bit_depth < 1) return LIBIMAGE_PNG_ERROR_IHDR_BIT_DEPTH;
	if(h->bit_depth != 1 && h->bit_depth & 1) return LIBIMAGE_PNG_ERROR_IHDR_BIT_DEPTH;
	if(h->interlace_method < 0 && h->interlace_method > 1) return LIBIMAGE_PNG_ERROR_IHDR_INTERLACE;

	switch(h->colour_type) {
		case PNG_COLOR_TYPE_GREYSCALE: {
			if(h->bit_depth != 1 && (IS_POWER_OF_TWO(h->bit_depth) == 0)) return LIBIMAGE_PNG_ERROR_IHDR_BIT_DEPTH_COMBINATION;

		} break;
		case PNG_COLOR_TYPE_TRUE_COLOUR_WITH_ALPHA:
		case PNG_COLOR_TYPE_GREYSCALE_WITH_ALPHA:
		case PNG_COLOR_TYPE_TRUECOLOUR: {
			if(h->bit_depth != 8 && h->bit_depth != 16) return LIBIMAGE_PNG_ERROR_IHDR_BIT_DEPTH_COMBINATION;
		} break;
		case PNG_COLOR_TYPE_INDEXED_COLOUR: {
			if(h->bit_depth != 1 && h->bit_depth > 8 && (IS_POWER_OF_TWO(h->bit_depth) == 0)) return LIBIMAGE_PNG_ERROR_IHDR_BIT_DEPTH_COMBINATION;
		} break;
	}

	i->color_type = h->colour_type;
	return 0;
}

LibImagePngChunk read_png_chunk(LibImageDataReader *r)
{
LibImagePngChunk c = {0};
	
	c.data_len.i = u32_endian_swap(*(uint32_t*)read_from_reader(r)); 
	c.type.i     = u32_endian_swap(*(uint32_t*)peek_from_reader(r, 4));

	consume_bytes(r, 8);
	c.start_chunk_data = read_from_reader(r);
	c.end_chunk_data   = peek_from_reader(r, c.data_len.i - 1);
	consume_bytes(r, c.data_len.i);

	c.crc.i      = *(uint32_t*)read_from_reader(r);

	consume_bytes(r, 4);
	return c;
}

void process_ihdr_chunk(LibImagePngChunk *c, LibImageDataReader *r, LibImageImageInfo *info, uint8_t *compression_method)
{
int			validation_ret;
LibImagePngIHdr 	ihdr;

	ihdr = *(LibImagePngIHdr*)(c->start_chunk_data);
	validation_ret = validate_ihdr(&ihdr, info);
	if(validation_ret) {
		r->error = validation_ret; 
		return;
	}
	if(ihdr.compression_method == 0) compression_method = LIBIMAGE_DEFLATE_COMPRESSION;
	info->width 	= u32_endian_swap(ihdr.width);
	info->height	= u32_endian_swap(ihdr.height);

	if(info->width > LIBIMAGE_PNG_MAX_IMAGE_SIZE || info->height > LIBIMAGE_PNG_MAX_IMAGE_SIZE) {
		r->error = LIBIMAGE_PNG_ERROR_BIG_IMAGE;
		return;
	}

	if(info->width == 0 || info->height == 0) {
		r->error = LIBIMAGE_PNG_ERROR_ZERO_SIZE;
		return;
	}
#if 1
	print_ihdr(&ihdr);
#endif
}

void png_parse_uncompressed_block(LibImageZlibBuffer *buf, LibImageImageInfo *info)
{
int 		len,nlen, k;
uint8_t 	header[4], *tmp;
uint32_t 	mem_size = 1;

	len = nlen = k = 0;
	// Align to a byte boundary, as per spec.
	if(buf->code_buf_bits & 7) zbuf_get_n_bits(buf, buf->code_buf_bits & 7);
	while(buf->code_buf_bits) {
		header[k++] = (uint8_t) buf->code_buf & 255;
		buf->code_buf >>= 8;
		buf->code_buf_bits -= 8;
	}
	if(buf->code_buf_bits < 0) {
		info->error = LIBIMAGE_PNG_ERROR_CORRUPTED_FILE;
		return;
	}
	while(k < 4) header[k++] = zbuf_get_byte(buf);

	len 	|= header[1] * 256 + header[0];
	nlen	 = header[3] * 256 + header[2];

	if(len != ~(nlen)) {
		info->error = LIBIMAGE_PNG_ERROR_CORRUPTED_FILE;
		return;
	}
	if(buf->buf + len > buf->buf_end) {
		info->error = LIBIMAGE_PNG_ERROR_CORRUPTED_FILE;
		return;
	}
	write_uncompressed_data(buf, info, len, NULL);
}

void png_parse_huffman_fixed_block(LibImageZlibBuffer *buf, LibImageImageInfo *info)
{
int 		hlit, hdist, i, bit_count, bit_index, go_until_this_value;
uint32_t 	lit_bit_count[][2] = { {143,8}, {255,9}, {279, 7}, {287, 8}, {319, 5}}; // Encode distance in the same array after 287, 319 is 287 + hdist

	bit_index = 0;
	hlit	  = 288;
	hdist	  = 32;

	for(i = 0; i < STATIC_ARRAY_SIZE(lit_bit_count); i++) {
		bit_count = lit_bit_count[i][1];
		go_until_this_value = lit_bit_count[i][0];	
		while(bit_index <= go_until_this_value) {
			zbuf_append_to_sliding_window(buf, (uint8_t*)&bit_count, sizeof(bit_count));
			bit_index++;
		}
	}
	decompress_huffman_block(buf, info, hlit, hdist);
}

void png_parse_huffman_dynamic_block(LibImageZlibBuffer *buf, LibImageImageInfo *info)
{
const uint8_t 		code_len_alphabet[19] = { 16,17,18,0,8,7,9,6,10,5,11,4,12,3,13,2,14,1,15 }; // Order from spec, don't ask me
uint8_t 		code_len_lens[19];
int 			hlit, hdist, hclen, n, run_len, run_val, actual_len, encoded_len, total;
LibImageHuffman 	huff;


	hlit  = zbuf_get_n_bits(buf, 5) + 257;
	hdist = zbuf_get_n_bits(buf, 5) + 1;
	hclen = zbuf_get_n_bits(buf, 4) + 4;

	total = hlit + hdist;

	libimage_printf("HLIT  %d\n", hlit);
	libimage_printf("HDIST %d\n", hdist);
	libimage_printf("HCLEN %d\n", hclen);
	libimage_printf("TOTAL %d\n", total);

	for(n = 0; n < hclen; n++) {
		code_len_lens[code_len_alphabet[n]] = (uint8_t)zbuf_get_n_bits(buf, 3);
	}

	huff = new_huffman(7);
	build_huffman(buf, &huff, code_len_lens, STATIC_ARRAY_SIZE(code_len_lens));
	n    = 0;
	while(n < total) {
		encoded_len = decode_huffman(&huff, buf);
		if(encoded_len < 0 || encoded_len >= 19) {
			info->error = LIBIMAGE_PNG_HUFFMAN_BAD_CODE_LENGTHS;
			return;
		}

		if(encoded_len <= 15) run_val = encoded_len;
		else if(encoded_len == 16) {
			run_len = zbuf_get_n_bits(buf, 2) + 3;
			run_val = buf->sliding_window[n - 1];
		}
		else if(encoded_len == 17) {
			run_len = zbuf_get_n_bits(buf, 3) + 3;
		} 
		else if(encoded_len == 18) {
			run_len = zbuf_get_n_bits(buf, 7) + 11;
		} 
		else {
			libimage_printf("encoded_len is %d [%d].\n", encoded_len, __LINE__);
			info->error = LIBIMAGE_PNG_ERROR_CORRUPTED_FILE;
			return;
		}

		while(encoded_len--) {
			zbuf_append_to_sliding_window(buf, (uint8_t*)&run_val, sizeof(run_val));
			n++;
		}
	}

	decompress_huffman_block(buf, info, hlit, hdist);
	free_huffman_entries(&huff);
}

void handle_png_data(LibImageImageInfo *info)
{
LibImageZlibBuffer	zlib_buf;
off_t			i, contents_size;
uint8_t			type, end;

	zbuf_init(&zlib_buf, info->compressed_data, info->cd_offset, 1);
	zbuf_parse_header(&zlib_buf);

	if(zlib_buf.error) {
		info->error = zlib_buf.error;
		return;
	}

	end = 0;
	do {
		end	= zbuf_get_n_bits(&zlib_buf, 1);
		type 	= zbuf_get_n_bits(&zlib_buf, 2);

		if(type == 3) {
			libimage_printf("Got type %d for deflate\n", type);	
			continue;
		}
		if(type == 0) {
			png_parse_uncompressed_block(&zlib_buf, info);
		} else if(type == 1) {
			png_parse_huffman_fixed_block(&zlib_buf, info);
		} else {
			png_parse_huffman_dynamic_block(&zlib_buf, info);
		}
		if(info->error) return;
	} while( !end);
	zbuf_deinit(&zlib_buf);
}

void libimage_process_png(LibImageDataReader *r, LibImageImageInfo *info)
{
LibImagePngChunk 	chunk;
uint8_t			*tmp_ptr, compression_method = -1;
uint8_t			first_chunk = 1, got_idat_chunk = 0, got_plte_chunk = 0, got_gama_chunk=0;
uint64_t		loop_count;
uint32_t		state, size_data;
uint32_t		crc32;

	size_data = 0;
	for(loop_count = 0; loop_count < MAXIMUM_LOOP_ALLOWED; loop_count++) {
		chunk = read_png_chunk(r);
#ifdef LIBIMAGE_PNG_CHECK_CRC
		crc32  = crc(chunk.type.b, sizeof(chunk.type.b) + chunk.data_len.i);
		if(chunk.crc.i != crc32) {
			libimage_printf("Corrupted file crc-chunk %x crc-calc %x\n", chunk.crc.i, crc32);
			r->error = LIBIMAGE_PNG_ERROR_CRC_NOT_MATCH;
			return;
		}
#endif
		switch(chunk.type.i) {
			case LIBIMAGE_PNG_TYPE('I','H','D','R'): {
				if(!first_chunk) {
					r->error = LIBIMAGE_PNG_ERROR_MULTIPLE_IHDR;
					return;
				}
				if(chunk.data_len.i != 13) {
					r->error = LIBIMAGE_PNG_ERROR_CORRUPT_IHDR;
					return;
				}
				process_ihdr_chunk(&chunk, r, info, &compression_method);
				if(r->error) return;
				first_chunk = 0;
			} break;
			case LIBIMAGE_PNG_TYPE('g','A','M','A'): {
				if(first_chunk) {
					r->error = LIBIMAGE_PNG_ERROR_IHDR_NOT_FOUND;
					return;
				}
				if(got_plte_chunk) {
					r->error = LIBIMAGE_PNG_ERROR_GAMA_AFTER_PLTE;
					return;
				}
				if(got_gama_chunk) {
					r->error = LIBIMAGE_PNG_ERROR_MULTIPLE_GAMA;
					return;
				}
				info->gamma = u32_endian_swap(*(uint32_t*)chunk.start_chunk_data);
				got_gama_chunk=1;
			} break;
			case LIBIMAGE_PNG_TYPE('P','L','T','E'): {
				if(first_chunk) {
					r->error = LIBIMAGE_PNG_ERROR_IHDR_NOT_FOUND;
					return;
				}
				if(info->color_type == PNG_COLOR_TYPE_GREYSCALE
					  	|| info->color_type == PNG_COLOR_TYPE_GREYSCALE_WITH_ALPHA) {
					r->error = LIBIMAGE_PNG_ERROR_UNEXPECTED_PLTE;
					return;
				}
				got_plte_chunk = 1;
			} break;
			case LIBIMAGE_PNG_TYPE('I','D','A','T'): {
				if(first_chunk) {
					r->error = LIBIMAGE_PNG_ERROR_IHDR_NOT_FOUND;
					return;
				}
				got_idat_chunk = 1;
				if(chunk.data_len.i > ( 1 << 30)) {
					r->error = LIBIMAGE_PNG_ERROR_IDAT_SIZE_LIMIT;
					return;
				}
				if(info->cd_offset + chunk.data_len.i > size_data) {
					if(size_data == 0) size_data = chunk.data_len.i > IDAT_DEFAULT_BLOCK_SIZE ? chunk.data_len.i : IDAT_DEFAULT_BLOCK_SIZE;
					while(info->cd_offset + chunk.data_len.i > size_data) size_data *= 2;
					tmp_ptr = realloc(info->compressed_data, size_data);
					if(tmp_ptr == NULL) {
						r->error = LIBIMAGE_ERROR_MEMORY_ERROR;
						return;
					}
					info->compressed_data = tmp_ptr;
					info->cd_size = size_data;
				}
				// Append to the buffer
				copy_to_buffer(info->compressed_data + info->cd_offset, chunk.start_chunk_data, chunk.end_chunk_data - chunk.start_chunk_data);
				info->cd_offset += chunk.data_len.i;
			} break;
			case LIBIMAGE_PNG_TYPE('I','E','N','D'): {
				if(first_chunk) {
					r->error = LIBIMAGE_PNG_ERROR_IHDR_NOT_FOUND;
					return;
				}
				if(!got_idat_chunk) {
					r->error = LIBIMAGE_PNG_ERROR_NO_IDAT;
					return;
				}
				if(info->color_type == PNG_COLOR_TYPE_INDEXED_COLOUR && !got_plte_chunk) {
					r->error = LIBIMAGE_PNG_ERROR_NO_PLTE;
					return;
				}
				// At the end process the actual image.
				handle_png_data(info);
				if(info->error) 
					r->error = info->error;
				libimage_printf("Color type %d\n", info->color_type); //TODO ern
				libimage_printf("Got to the end of the file\n");
				return;
			} break;
			default: {
				libimage_printf("Type is %s(%x) which is not supported.\n", chunk.type.b, chunk.type.i);
				r->error = LIBIMAGE_PNG_ERROR_INVALID_FILE;
				goto bail;
			} break;
		}
	}
bail:
}

