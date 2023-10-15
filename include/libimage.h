#ifndef  __LIBIMAGE__H__
#define  __LIBIMAGE__H__

void *libimage_process_data(char *data, unsigned int *width, unsigned int *height, int *error);
void libimage_error_code_to_msg(unsigned char *buffer, int buffer_size, int error);

#endif
