/*	2023 - libimage by Ernesto Bayma 
 *	
 *	Reference for png: https://www.w3.org/TR/2003/REC-PNG-20031110 and http://www.libpng.org/pub/png/spec/1.2/PNG-Structure.html
 *
 *
 */
/*
*	Zlib data format
*	Byte 0 -> CMF
*	Byte 1 -> FLG
*	if FLG.FDICT set
*
*	4 bytes for DICTID
*	Then Compressed data
*	Then 4 bytes for the ADLER32
*
*	Any data that appear after ADLER32 are not part of the zlib stream
*
*	CMF ( Compression method and flags )
*	A byte divided between 4 bits for compression method and 4 bits for information field
*
*	CM ( Compression method) 
*		CM = 8 Represents the deflate compression with a window size up to 32k, this is used by gzip and PNG.
*		CM = 15 is Reserved.
*	CINFO ( Compression info )
*		CM = 8 , CINFO is the base 2 log of the LZ77 window size minus eight ( EX: CINFO=7 indicates a 32K window size).
*			values above 7 is not allowed 
*	FLG (Flags)
*		bits 0 to 4 FCHECK ( check bits for CMF and FLG )
*		bit 5 	    FDICT (preset dictionary )
*		bits 6 to 7 FLEVEL (compression level )
*
*		The FCHECK value must be such that CMF and FLG, when viewed as a uint16_t stored in Network order (CMF*256 + FLG ) is a
*		multiple of 31.
*
*		FDICT ( Preset dictionary )
*			If FDICT is set, a DICT dictionary identifier is present immediately afther the FLG byte. The dictionary is a sequence of bytes
*			which are initially fed to the compressor without producing any compressed output. DICT is the Adler-32 checksum of this sequence of 
*			bytes. The decompressor can use this identifier to determine which dictionary has been used by the compressor.
*		FLEVEL ( Compression level )
* 			Not needed for decompression
* 		Compressed data
* 			For the method 8, data is stored in the deflate method.
* 		ADLER32 (Adler-32 checksum)
* 			This contains a checksum value of the uncompressed data -> Validation of the decompression.
*	 			
*		A compliant decompressor must check CMF, FLG, and ADLER32, and
*		provide an error indication if any of these have incorrect values.
*		A compliant decompressor must give an error indication if CM is
*		not one of the values defined in this specification (only the
*		value 8 is permitted in this version), since another value could
*		indicate the presence of new features that would cause subsequent
*		data to be interpreted incorrectly.  A compliant decompressor must
*		give an error indication if FDICT is set and DICTID is not the
*		identifier of a known preset dictionary.  A decompressor may
*		ignore FLEVEL and still be compliant.  When the zlib data format
*		is being used as a part of another standard format, a compliant
*		decompressor must support all the preset dictionaries specified by
*		the other format. When the other format does not use the preset
*		dictionary feature, a compliant decompressor must reject any
*		stream in which the FDICT flag is set.
* 
 */


#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <limits.h>
#include <inttypes.h>

#define STATIC_ARRAY_SIZE(array) (int)((sizeof((array))/sizeof((array[0]))))

enum {
	LIBIMAGE_PNG_ERROR_HEADER = 1,
	LIBIMAGE_PNG_ERROR_IHDR_NOT_FOUND,
	LIBIMAGE_PNG_ERROR_INVALID_FILE,
	LIBIMAGE_PNG_ERROR_IHDR_BIT_DEPTH,
	LIBIMAGE_PNG_ERROR_IHDR_COLOUR_TYPE,
	LIBIMAGE_PNG_ERROR_IHDR_BIT_DEPTH_COMBINATION,
	LIBIMAGE_PNG_ERROR_CRC_NOT_MATCH,
	LIBIMAGE_PNG_ERROR_MULTIPLE_IHDR,
	LIBIMAGE_PNG_ERROR_NO_IDAT,
	LIBIMAGE_PNG_ERROR_NO_PLT,
	LIBIMAGE_PNG_ERROR_UNEXPECTED_PLTE,
	LIBIMAGE_ERROR_TYPE_NOT_SUPPORTED
};

enum {
	LIBIMAGE_TYPE_PNG = 1
};


uint32_t u32_bswap(uint32_t value);

#define PNG_COLOR_TYPE_GREYSCALE 		0 // Each pixel is a greyscale sample
#define PNG_COLOR_TYPE_TRUECOLOUR		2 // Each pixel is a R,G,B triple
#define PNG_COLOR_TYPE_INDEXED_COLOUR		3 // Each pixel is a palette index; a PLTE chunk shall appear.
#define PNG_COLOR_TYPE_GREYSCALE_WITH_ALPHA	4 // Each pixel is a greyscale sample followed by an alpha sample.
#define PNG_COLOR_TYPE_TRUE_COLOUR_WITH_ALPHA	6 // Each pixel is an R,G,B triple followed by an alpha sample.

#define CHUNK_LENGTH_BYTES		4
#define CHUNK_TYPE_BYTES		4
#define CHUNK_CRC_BYTES			4
#define MAXIMUM_LOOP_ALLOWED		UINT64_MAX - 1

#define IS_POWER_OF_TWO(num)		(((int)(num)) & ((((int)num) - 1)))
#define LIBIMAGE_PNG_TYPE(a,b,c,d) 	(((uint32_t)((a) << 24)) + ((uint32_t)((b) << 16)) + ((uint32_t)((c) << 8)) + (uint32_t)(d))

static uint8_t png_file_sig[]   = { 0x89, 0x50, 0x4e, 0x47, 0x0d, 0x0a, 0x1a, 0x0a };

// A valid PNG datastream shall begin with a PNG signature, immediately followed by an IHDR chunk, then one or more IDAT chunks, and shall end with an IEND chunk. Only one IHDR chunk and one IEND chunk are allowed in a PNG datastream.
/*
*
* 	3.2. Chunk layout
* 	Length
*     		A 4-byte unsigned integer giving the number of bytes in the chunk's data field. The length counts only the data field, not itself, the chunk type code, or the CRC. Zero is a valid length. Although encoders and decoders should treat the length as unsigned, its value must not exceed 231 bytes.
*
*     Chunk Type
*         	A 4-byte chunk type code. For convenience in description and in examining PNG files, type codes are restricted to consist of uppercase and lowercase ASCII letters 
*         	(A-Z and a-z, or 65-90 and 97-122 decimal). However, encoders and decoders must treat the codes as fixed binary values, not character strings. 
*         	For example, it would not be correct to represent the type code IDAT by the EBCDIC equivalents of those letters. Additional naming conventions for chunk types are discussed in the next section.
*         Chunk Data
*             	The data bytes appropriate to the chunk type, if any. This field can be of zero length.
*         CRC
*             	A 4-byte CRC (Cyclic Redundancy Check) calculated on the preceding bytes in the chunk, including the chunk type code and chunk data fields, but not including the length field. The CRC is always present, even for chunks containing no data.
*               The chunk data length can be any number of bytes up to the maximum; therefore, implementors cannot assume that chunks are aligned on any boundaries larger than bytes.
*
*         Chunks can appear in any order, subject to the restrictions placed on each chunk type. (One notable restriction is that IHDR must appear first and IEND must appear last; thus the IEND chunk serves as an end-of-file marker.) Multiple chunks of the same type can appear, but only if specifically permitted for that type. 
*/
typedef struct libimage_png_chunk {
	union {
		uint8_t b[4]; // Is unsigned but should not be bigger than 2^31 - 1 bytes.
		uint32_t  i;
	} data_len;
	union {
		uint8_t b[4]; // Restricted to the decimals values 65 to 90 and 97 to 122
		uint32_t  i;    
	} type;

	uint8_t	*start_chunk_data; 	// Can be NULL 
	uint8_t *end_chunk_data;	// Can be NULL
	
	union {	
		uint8_t b[4];
		uint32_t  i;
	} crc;      		// calculated on the preceding bytes in the chunk, including the chunk type field and chunk data fields, but not including the length field.
				
	struct libimage_png_chunk *next;
} LibImagePngChunk;


typedef struct libimage_image_info {
	uint32_t width;
	uint32_t height;
	uint32_t gamma;
	uint8_t  *data;
	uint8_t  color_type;
} LibImageImageInfo;

/*
 *
 * Width and height give the image dimensions in pixels. They are PNG four-byte uint32_tegers. Zero is an invalid value.
 *
 * Bit depth is a single-byte integer giving the number of bits per sample or per palette index (not per pixel). Valid values are 1, 2, 4, 8, and 16, although not all values are allowed for all colour types.
 *
 * Colour type is a single-byte integer that defines the PNG image type. Valid values are 0, 2, 3, 4, and 6.
 *
 * Bit depth restrictions for each colour type are imposed to simplify implementations and to prohibit combinations that do not compress well
*/

typedef struct libimage_png_ihdr {
	uint32_t width;
	uint32_t height;
	uint8_t bit_depth;
	uint8_t colour_type;
	uint8_t compression_method;
	uint8_t filter_method;
	uint8_t interlace_method;
} LibImagePngIHdr;
static int validate_ihdr(LibImagePngIHdr *h, LibImageImageInfo *i);

typedef struct libimage_png_plte {
	char rgb[3];
} LibImagePngPLTE;

typedef struct libimage_data_reader {
	uint8_t 	*data;
	uint16_t	type;
	uint16_t	error;
	uint32_t	cursor;
	uint32_t	peek_cursor;
} LibImageDataReader;

static void consume_bytes(LibImageDataReader *r, int n)
{
	r->cursor+=n;
}

static uint8_t *read_from_reader(LibImageDataReader *r)
{
	return r->data + r->cursor;
}

static uint8_t *peek_from_reader(LibImageDataReader *r, int n)
{
	return r->data + r->cursor + n;
}

static void print_ihdr(LibImagePngIHdr *h)
{
	printf("Width  %x\n", 		h->width);
	printf("Height %x\n", 		h->height);
	printf("Bit depth %x\n", 	h->bit_depth);
	printf("Color type %x\n", 	h->colour_type);
	printf("Compression method %x\n", h->compression_method);
	printf("Filter method %x\n", h->filter_method);
	printf("Interlace method %x\n", h->interlace_method);
}
/*
*
*    CRC algorithm
*
*    Chunk CRCs are calculated using standard CRC methods with pre and post conditioning, as defined by ISO 3309 [ISO-3309] or ITU-T V.42 [ITU-T-V42]. 
*    The CRC polynomial employed is x^32+x^26+x^23+x^22+x^16+x^12+x^11+x^10+x^8+x^7+x^5+x^4+x^2+x+1
*
*    The 32-bit CRC register is initialized to all 1's, and then the data from each byte is processed from the least significant bit (1) to the most significant bit (128). 
*    After all the data bytes are processed, the CRC register is inverted (its ones complement is taken). 
*    This value is transmitted (stored in the file) MSB first. For the purpose of separating into bytes and ordering, the least significant bit of the 32-bit CRC is defined 
*    to be the coefficient of the x31 term. 
*
*/
#define CRC_POLYNOMIAL 		0xebd88320L
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
		c = (unsigned long) n;
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
uint32_t crc(uint8_t *buf, int len, int offset_bytes)
{
uint32_t result;
int	 n;
	if (!crc_table_computed)
		make_crc_table();

	result = 0;
	result = (~result) & ALL_32_BITS_SET;

	while(len >= 8) {
		len -= 8;
		result = (result >> 8) ^ crc_table[(result ^ *buf++) & 0xff];
		result = (result >> 8) ^ crc_table[(result ^ *buf++) & 0xff];
		result = (result >> 8) ^ crc_table[(result ^ *buf++) & 0xff];
		result = (result >> 8) ^ crc_table[(result ^ *buf++) & 0xff];
		result = (result >> 8) ^ crc_table[(result ^ *buf++) & 0xff];
		result = (result >> 8) ^ crc_table[(result ^ *buf++) & 0xff];
		result = (result >> 8) ^ crc_table[(result ^ *buf++) & 0xff];
		result = (result >> 8) ^ crc_table[(result ^ *buf++) & 0xff];
	}

	while(len) {
		len--;	
		result = (result >> 8) ^ crc_table[(result ^ *buf++) & 0xff];
	}

	result = result ^ ALL_32_BITS_SET;
	result = u32_bswap(result);
	return result;
}

static int check_png_signature(LibImageDataReader *r)
{
uint64_t sig_to_check, correct_sig;
	correct_sig 	= *(uint64_t*)png_file_sig;
	sig_to_check	= *(uint64_t*)r->data;	

	if(sig_to_check != sig_to_check) return -1;

	consume_bytes(r, sizeof(correct_sig));
	return 0;
}

static int check_data_header(LibImageDataReader *r)
{
	if(check_png_signature(r) == 0) {
		r->type = LIBIMAGE_TYPE_PNG;
	} else {
		r->error = LIBIMAGE_ERROR_TYPE_NOT_SUPPORTED;
		return -1;
	}
	return 0;	
}

static int validate_ihdr(LibImagePngIHdr *h, LibImageImageInfo *i)
{
	if(h == NULL) return 0;
	
	if(h->colour_type < 0 && h->colour_type > 6) return LIBIMAGE_PNG_ERROR_IHDR_COLOUR_TYPE;
	if(h->colour_type == 1 || h->colour_type == 5) return LIBIMAGE_PNG_ERROR_IHDR_COLOUR_TYPE;

	if(h->bit_depth > 16 && h->bit_depth < 1) return LIBIMAGE_PNG_ERROR_IHDR_BIT_DEPTH;
	if(h->bit_depth != 1 && h->bit_depth & 1) return LIBIMAGE_PNG_ERROR_IHDR_BIT_DEPTH;

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
	
	c.data_len.i = u32_bswap(*(uint32_t*)read_from_reader(r)); 
	c.type.i     = u32_bswap(*(uint32_t*)peek_from_reader(r, 4));

	consume_bytes(r, 8);
	c.start_chunk_data = read_from_reader(r);
	c.end_chunk_data   = peek_from_reader(r, c.data_len.i - 1);
	consume_bytes(r, c.data_len.i);

	c.crc.i      = u32_bswap(*(uint32_t*)read_from_reader(r));

	consume_bytes(r, 4);
	return c;
}

static void process_ihdr_chunk(LibImagePngChunk *c, LibImageDataReader *r, LibImageImageInfo *info)
{
int			validation_ret;
LibImagePngIHdr 	ihdr;

	ihdr = *(LibImagePngIHdr*)(c->start_chunk_data);
	validation_ret = validate_ihdr(&ihdr, info);
	if(validation_ret) {
		r->error = validation_ret; 
		return;
	}
	info->width 	= u32_bswap(ihdr.width);
	info->height	= u32_bswap(ihdr.height);
#if 1
	print_ihdr(&ihdr);
#endif
	c->next 	= NULL;
}

uint32_t u32_bswap(uint32_t value)
{
int  	i, total;	
union   { uint32_t i; char b[4]; } test = { 0x01020304 };

	if(test.b[0] == 1) return value;

	// Praying that the compiler just do a bswap :^)
	value = ((value & 0xff000000u) >> 24 ) | ((value & 0x00ff0000u) >> 8)
	| ((value & 0x0000ff00u) << 8  ) | ((value & 0x000000ffu) << 24);

	return value;	
}

void libimage_error_code_to_msg(char *buffer, int buffer_size, int error)
{
char *msg;

	switch(error) {
	case LIBIMAGE_PNG_ERROR_HEADER: msg = "Data has wrong file signature in the header for a PNG file."; break;
	case LIBIMAGE_PNG_ERROR_INVALID_FILE: msg = "Data has invalid sequence for a PNG file."; break;
	case LIBIMAGE_PNG_ERROR_IHDR_NOT_FOUND: msg = "Data don't start with the IHDR chunk which need to be the first chunk for a PNG file."; break;
	case LIBIMAGE_PNG_ERROR_IHDR_BIT_DEPTH: msg = "Data has a invalid value for the bit depth field on IHDR chunk."; break;	
	case LIBIMAGE_PNG_ERROR_IHDR_COLOUR_TYPE: msg = "Data has a invalid value for the colour type field on IHDR chunk."; break;	
	case LIBIMAGE_PNG_ERROR_IHDR_BIT_DEPTH_COMBINATION: msg = "Data has a invalid combination between bit depth and colour type on IHDR chunk."; break;	
	case LIBIMAGE_PNG_ERROR_CRC_NOT_MATCH: msg = "Data has a calculated crc that don't match the crc on the chunk."; break;
	case LIBIMAGE_PNG_ERROR_MULTIPLE_IHDR: msg = "Data has multiple IHDR chunks. Which is not supported by the PNG spec."; break;
	case LIBIMAGE_PNG_ERROR_NO_IDAT: msg = "Data has no IDAT chunk for a PNG file."; break;
	case LIBIMAGE_PNG_ERROR_NO_PLT:  msg = "Expected a PLTE chunk based on Image type field from IHDR, but none was found."; break; 
	case LIBIMAGE_PNG_ERROR_UNEXPECTED_PLTE:  msg = "Got a PLTE but chunk Image type field from IHDR don't support it."; break; 
	case LIBIMAGE_ERROR_TYPE_NOT_SUPPORTED: msg = "Data has not supported header info."; break;
	default: msg = "Unknown error. RUN."; break;
	}

	snprintf(buffer, buffer_size, "%s", msg);
}

void libimage_process_png(LibImageDataReader *r, LibImageImageInfo *info)
{
LibImagePngChunk 	chunk;
uint32_t		first_chunk = 1, got_idat_chunk = 0, got_plte_chunk = 0;
uint64_t		loop_count;

	for(loop_count = 0; loop_count < MAXIMUM_LOOP_ALLOWED; loop_count++) {
		chunk = read_png_chunk(r);
		switch(chunk.type.i) {
			case LIBIMAGE_PNG_TYPE('I','H','D','R'): {
				if(!first_chunk) {
					r->error = LIBIMAGE_PNG_ERROR_MULTIPLE_IHDR;
					return;
				}
				process_ihdr_chunk(&chunk, r, info);
				first_chunk = 0;
			} break;
			case LIBIMAGE_PNG_TYPE('g','A','M','A'): {
				if(first_chunk) {
					r->error = LIBIMAGE_PNG_ERROR_IHDR_NOT_FOUND;
					return;
				}
				info->gamma = u32_bswap(*(uint32_t*)chunk.start_chunk_data);
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
				fprintf(stderr, "Implement the real thing\n"); //TODO ern
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
					r->error = LIBIMAGE_PNG_ERROR_NO_PLT;
					return;
				}
				fprintf(stderr, "Got to the end of the file\n");
				return;
			} break;			 
			default:
				fprintf(stderr, "Type is %s(%x) which is not supported.\n", chunk.type.b, chunk.type.i); //TODO ern
				goto bail;
			break;
		}
	}
bail:
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


	if(width) 	*width = info.width;
	if(height)	*height = info.height;

	return data; //For now
}
