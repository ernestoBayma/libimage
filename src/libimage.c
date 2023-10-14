#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define STATIC_ARRAY_SIZE(array) (int)((sizeof((array))/sizeof((array[0]))))

enum {
	LIBIMAGE_PNG_ERROR_HEADER = 1,
	LIBIMAGE_PNG_ERROR_INVALID_FILE,
	LIBIMAGE_PNG_ERROR_IHDR_BIT_DEPTH,
	LIBIMAGE_PNG_ERROR_IHDR_COLOUR_TYPE,
	LIBIMAGE_PNG_ERROR_IHDR_BIT_DEPTH_COMBINATION
};

#define CHUNK_LENGTH_BYTES	4
#define CHUNK_TYPE_BYTES	4
#define CHUNK_CRC_BYTES		4
#define IS_POWER_OF_TWO(num)	(((int)(num)) & ((((int)num) - 1)))

static unsigned char png_file_sig[]   = { 0x89, 0x50, 0x4e, 0x47, 0x0d, 0x0a, 0x1a, 0x0a };
static unsigned char png_ihdr_chunk[] = { 73, 72, 68, 82 };	
static unsigned char png_iend_chunk[] = { 73, 69, 78, 68 };

/*
 * 3.4. CRC algorithm
 *
 * Chunk CRCs are calculated using standard CRC methods with pre and post conditioning, as defined by ISO 3309 [ISO-3309] or ITU-T V.42 [ITU-T-V42]. The CRC polynomial employed is
 *
 *    x^32+x^26+x^23+x^22+x^16+x^12+x^11+x^10+x^8+x^7+x^5+x^4+x^2+x+1
 *
 *    The 32-bit CRC register is initialized to all 1's, and then the data from each byte is processed from the least significant bit (1) to the most significant bit (128). After all the data bytes are processed, the CRC register is inverted (its ones complement is taken). This value is transmitted (stored in the file) MSB first. For the purpose of separating into bytes and ordering, the least significant bit of the 32-bit CRC is defined to be the coefficient of the x31 term. 
*/

/*
 * 3.2. Chunk layout
 *
 *
 * Length
 *     A 4-byte unsigned integer giving the number of bytes in the chunk's data field. The length counts only the data field, not itself, the chunk type code, or the CRC. Zero is a valid length. Although encoders and decoders should treat the length as unsigned, its value must not exceed 231 bytes.
 *
 *     Chunk Type
 *         A 4-byte chunk type code. For convenience in description and in examining PNG files, type codes are restricted to consist of uppercase and lowercase ASCII letters (A-Z and a-z, or 65-90 and 97-122 decimal). However, encoders and decoders must treat the codes as fixed binary values, not character strings. For example, it would not be correct to represent the type code IDAT by the EBCDIC equivalents of those letters. Additional naming conventions for chunk types are discussed in the next section.
 *
 *         Chunk Data
 *             The data bytes appropriate to the chunk type, if any. This field can be of zero length.
 *
 *             CRC
 *                 A 4-byte CRC (Cyclic Redundancy Check) calculated on the preceding bytes in the chunk, including the chunk type code and chunk data fields, but not including the length field. The CRC is always present, even for chunks containing no data. See CRC algorithm.
 *
 *                 The chunk data length can be any number of bytes up to the maximum; therefore, implementors cannot assume that chunks are aligned on any boundaries larger than bytes.
 *
 *                 Chunks can appear in any order, subject to the restrictions placed on each chunk type. (One notable restriction is that IHDR must appear first and IEND must appear last; thus the IEND chunk serves as an end-of-file marker.) Multiple chunks of the same type can appear, but only if specifically permitted for that type. 
 */

// A valid PNG datastream shall begin with a PNG signature, immediately followed by an IHDR chunk, then one or more IDAT chunks, and shall end with an IEND chunk. Only one IHDR chunk and one IEND chunk are allowed in a PNG datastream.

typedef struct libimage_png_chunk {
	union {
		unsigned char b[4];
		unsigned int  i;
	} data_len;
	union {
		unsigned char b[4]; // Restricted to the decimals values 65 to 90 and 97 to 122
		unsigned int  i;    
	} type;
	void *chunk_data; // This can be zero
	unsigned char crc[4];      // calculated on the preceding bytes in the chunk, including the chunk type field and chunk data fields, but not including the length field.
} LibImagePngChunk;


/*
 *
 * Width and height give the image dimensions in pixels. They are PNG four-byte unsigned integers. Zero is an invalid value.
 *
 * Bit depth is a single-byte integer giving the number of bits per sample or per palette index (not per pixel). Valid values are 1, 2, 4, 8, and 16, although not all values are allowed for all colour types.
 *
 * Colour type is a single-byte integer that defines the PNG image type. Valid values are 0, 2, 3, 4, and 6.
 *
 * Bit depth restrictions for each colour type are imposed to simplify implementations and to prohibit combinations that do not compress well
*/

typedef struct libimage_png_ihdr {
	unsigned int width;
	unsigned int height;
	unsigned char bit_depth;
	unsigned char colour_type;
	unsigned char compression_method;
	unsigned char filter_method;
	unsigned char interlace_method;
} LibImagePngIHdr;

static int validate_ihdr(LibImagePngIHdr *h)
{
	if(h == NULL) return 0;
	
	if(h->colour_type < 0 && h->colour_type > 6) return LIBIMAGE_PNG_ERROR_IHDR_COLOUR_TYPE;
	if(h->colour_type == 1 || h->colour_type == 5) return LIBIMAGE_PNG_ERROR_IHDR_COLOUR_TYPE;

	if(h->bit_depth > 16 && h->bit_depth < 1) return LIBIMAGE_PNG_ERROR_IHDR_BIT_DEPTH;
	if(h->bit_depth != 1 && h->bit_depth & 1) return LIBIMAGE_PNG_ERROR_IHDR_BIT_DEPTH;

	switch(h->colour_type) {
		case 0: {
			if(h->bit_depth != 1 && (IS_POWER_OF_TWO(h->bit_depth) == 0)) return LIBIMAGE_PNG_ERROR_IHDR_BIT_DEPTH_COMBINATION;

		} break;
		case 6:
		case 4:
		case 2: {
			if(h->bit_depth != 8 && h->bit_depth != 16) return LIBIMAGE_PNG_ERROR_IHDR_BIT_DEPTH_COMBINATION;
		} break;
		case 3: {
			if(h->bit_depth != 1 && h->bit_depth > 8 && (IS_POWER_OF_TWO(h->bit_depth) == 0)) return LIBIMAGE_PNG_ERROR_IHDR_BIT_DEPTH_COMBINATION;
		} break;
	}

	return 0;
}


void libimage_error_code_to_msg(char *buffer, int buffer_size, int error)
{
char *msg;

	switch(error) {
	case LIBIMAGE_PNG_ERROR_HEADER: msg = "Data has wrong file signature in the header for a PNG file."; break;
	case LIBIMAGE_PNG_ERROR_INVALID_FILE: msg = "Data has invalid sequence for a PNG file."; break;
	case LIBIMAGE_PNG_ERROR_IHDR_BIT_DEPTH: msg = "Data has a invalid value for the bit depth field on IHDR chunk."; break;	
	case LIBIMAGE_PNG_ERROR_IHDR_COLOUR_TYPE: msg = "Data has a invalid value for the colour type field on IHDR chunk."; break;	
	case LIBIMAGE_PNG_ERROR_IHDR_BIT_DEPTH_COMBINATION: msg = "Data has a invalid combination between bit depth and colour type on IHDR chunk."; break;	
	default: msg = "Unknown error. RUN."; break;
	}

	snprintf(buffer, buffer_size, "%s", msg);
}

static void print_chunk(LibImagePngChunk *c)
{
	printf("Data_len %x\n", c->data_len.i);
	printf("type %s\n", c->type.b);
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

void *libimage_process_png_data(unsigned char *data, int *width, int *height, int *error)
{
unsigned char 		*ptr;
LibImagePngChunk 	chunk;
LibImagePngIHdr 	ihdr;
unsigned int  		four_bytes;
int			validation_ret;

	if(width)  *width  = 0;
	if(height) *height = 0;
	if(error)  *error  = 0;

	if(memcmp(data, png_file_sig, STATIC_ARRAY_SIZE(png_file_sig)) != 0) {
		if(error) *error = LIBIMAGE_PNG_ERROR_HEADER;
		return NULL;
	}

	ptr = data + STATIC_ARRAY_SIZE(png_file_sig);
	chunk = *(LibImagePngChunk*)ptr;
	print_chunk(&chunk);

	four_bytes = *(unsigned int*)png_ihdr_chunk;
	if(chunk.type.i != four_bytes) {
		if(error) *error = LIBIMAGE_PNG_ERROR_INVALID_FILE;
		return NULL;
	}

	ptr += CHUNK_LENGTH_BYTES + CHUNK_TYPE_BYTES;

	ihdr = *(LibImagePngIHdr*)ptr;
	validation_ret = validate_ihdr(&ihdr);
	if(validation_ret) {
		if(error) *error = validation_ret;
		return NULL;
	}

	print_ihdr(&ihdr);

	return data; //For now
}
