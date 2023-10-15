/*
 *	http://www.schaik.com/pngsuite/ -> The "Official" test-suite for PNG.
 *
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <libimage.h>

#define ANSI_FONT_COL_RESET     "\x1b[0m"
#define FONT_COL_CUSTOM_RED     "\e[38;2;200;0;0m"
#define FONT_COL_CUSTOM_GREEN   "\e[38;2;0;200;0m"

#define LIBIMAGE_SUCCESS_MSG(msg, args...) (fprintf(stdout,FONT_COL_CUSTOM_GREEN "[INFO]: "  ANSI_FONT_COL_RESET msg"\n", args))
#define LIBIMAGE_ERROR_MSG(msg, args...)   (fprintf(stderr,FONT_COL_CUSTOM_RED   "[ERROR]: " ANSI_FONT_COL_RESET msg"\n", args))

char *readEntireFile(char *path, int *size)
{
FILE 	*f;
int   	ret, file_size;
char 	*contents;

	if(size) *size = 0;

	f = fopen(path, "r");
	if(f == NULL) return NULL;

	fseek(f, 0, SEEK_END);
	file_size = ftell(f);
	if(file_size <= 0) {
		fclose(f);
		return NULL;
	}

	contents = malloc(sizeof(char) * file_size);
	if(contents == NULL) {
		fclose(f);
		return NULL;
	}

	fseek(f, 0, SEEK_SET);
	ret = fread(contents, file_size, 1, f);

	if(ret == -1 && ferror(f) != 0) {
		fclose(f);
		free(contents);
		return NULL;
	}

	if(size) *size = file_size;

	fclose(f);
	return contents;
}

void usage(int code)
{
	fprintf(stderr, "<file_path_to_image>\n");
	exit(code);
}

int main(int argc, char **argv)
{
char 		*file_contents, *path, error_buffer[1024];
int  		size,  error;
unsigned int 	width, height;
void 		*ptr;

	if(argc < 2) usage(EXIT_FAILURE);	

	LIBIMAGE_SUCCESS_MSG("Starting png tests...", NULL);	

	error = 0;
	path = argv[1];

	file_contents = readEntireFile(path, &size);
	if(file_contents == NULL) {
		LIBIMAGE_ERROR_MSG("Could not read file", NULL);
		return -1;
	}

	ptr = libimage_process_data(file_contents, &width, &height, &error);
	if(error != 0) {
		libimage_error_code_to_msg(error_buffer, sizeof(error_buffer), error);
		fprintf(stderr, "%s\n", error_buffer);
	}

	fprintf(stderr, "Got width equal to %u\n", width);
	fprintf(stderr, "Got height equal to %u\n", height);

	free(file_contents);
	return 0;
}
