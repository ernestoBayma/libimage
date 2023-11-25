/*	2023 - libimage by Ernesto Bayma 
 *	
 *	Reference for png: https://www.w3.org/TR/2003/REC-PNG-20031110 and http://www.libpng.org/pub/png/spec/1.2/PNG-Structure.html
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

#ifndef __LIB_IMAGE_PNG_H__
#define __LIB_IMAGE_PNG_H__

#include <inttypes.h>

#ifndef LIBIMAGE_PNG_MAX_IMAGE_SIZE
#define LIBIMAGE_PNG_MAX_IMAGE_SIZE	( 1 << 24 )
#endif

#define PNG_COLOR_TYPE_GREYSCALE 		0 // Each pixel is a greyscale sample
#define PNG_COLOR_TYPE_TRUECOLOUR		2 // Each pixel is a R,G,B triple
#define PNG_COLOR_TYPE_INDEXED_COLOUR		3 // Each pixel is a palette index; a PLTE chunk shall appear.
#define PNG_COLOR_TYPE_GREYSCALE_WITH_ALPHA	4 // Each pixel is a greyscale sample followed by an alpha sample.
#define PNG_COLOR_TYPE_TRUE_COLOUR_WITH_ALPHA	6 // Each pixel is an R,G,B triple followed by an alpha sample.

#define LIBIMAGE_PNG_TYPE(a,b,c,d) 		(((uint32_t)((a) << 24)) + ((uint32_t)((b) << 16)) + ((uint32_t)((c) << 8)) + (uint32_t)(d))

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

typedef struct libimage_png_plte {
	char rgb[3];
} LibImagePngPLTE;

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
} LibImagePngChunk;

int validate_ihdr(LibImagePngIHdr *h, LibImageImageInfo *i);
void print_ihdr(LibImagePngIHdr *h);
LibImagePngChunk read_png_chunk(LibImageDataReader *r);
int check_png_signature(LibImageDataReader *r);
void process_ihdr_chunk(LibImagePngChunk *c, LibImageDataReader *r, LibImageImageInfo *info, uint8_t *compression_method);
void png_parse_uncompressed_block(LibImageZlibBuffer *buf, LibImageImageInfo *info);
void png_parse_huffman_fixed_block(LibImageZlibBuffer *buf, LibImageImageInfo *info);
void png_parse_huffman_dynamic_block(LibImageZlibBuffer *buf, LibImageImageInfo *info);
void handle_png_data(LibImageImageInfo *info);
void libimage_process_png(LibImageDataReader *r, LibImageImageInfo *info);
#endif
