#ifndef  __LIBIMAGE__H__
#define  __LIBIMAGE__H__

#include <inttypes.h>

void copy_to_buffer(uint8_t *dst, uint8_t *src, int size);
void *libimage_process_data(char *data, unsigned int *width, unsigned int *height, int *error);
void libimage_error_code_to_msg(unsigned char *buffer, int buffer_size, int error);

#endif
