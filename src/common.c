/*
*  ===================================================================	
*	2023 - libimage by Ernesto Bayma
*  ===================================================================
*/

#include "common.h"
#include <string.h>
#include <inttypes.h>

void consume_bytes(LibImageDataReader *r, int n)
{
	r->cursor+=n;
}

uint8_t *read_from_reader(LibImageDataReader *r)
{
	return r->data + r->cursor;
}

uint8_t *peek_from_reader(LibImageDataReader *r, int n)
{
	return r->data + r->cursor + n;
}

void copy_to_buffer(uint8_t *dst, uint8_t *src, int size) {
	memcpy(dst, src, size);	
}

uint32_t u32_endian_swap(uint32_t value)
{
int  	i, total;	
union   { uint32_t i; char b[4]; } test = { 0x01020304 };

	if(test.b[0] == 1) return value;

	// Praying that the compiler just do a bswap :^)
	value = ((value & 0xff000000u) >> 24 ) | ((value & 0x00ff0000u) >> 8)
	| ((value & 0x0000ff00u) << 8  ) | ((value & 0x000000ffu) << 24);

	return value;	
}

uint32_t bit_reverse(uint32_t value, int bits)
{
uint32_t res, i, inv;
	
	res = 0;
	for(i = 0; i <= (bits/2); i++) {
		inv = (bits - (i + 1));
		res |= ((value >> i) & 0x1) << inv;
		res |= ((value >> inv) & 0x1) << i;
	}
	
	return res;
}

void libimage_printf(const char *fmt, ...)
{
#ifdef LIBIMAGE_DEBUG 
va_list va;
	va_start(va, fmt);
	vfprintf(stderr, fmt, va);
	va_end(va);
#endif
}
