#ifndef _FILES_H_
#define _FILES_H_

#include <stdint.h>

typedef struct file_data{
    int size;
    void *data;
    char *filename;
} file_data;

extern file_data *file_load(char *filename);
extern void file_free(file_data * filedata);
// extern void free_image(uint8_t *image);
extern file_data *read_image(char *filename);

#endif // _FILES_H_