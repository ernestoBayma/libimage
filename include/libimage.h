#ifndef  __LIBIMAGE__H__
#define  __LIBIMAGE__H__

void *libimage_process_png_data(char *data, int *width, int *height, int *error);
void libimage_error_code_to_msg(unsigned char *buffer, int buffer_size, int error);

#endif
