#ifndef _FILES_H_
#define _FILES_H_

typedef struct file_data{
    int size;
    void *data;
    char *filename;
} file_data;

extern file_data *file_load(char *filename);
extern void file_free(file_data * filedata);
int get_image_fd(char *filename);
extern file_data *read_image(char *filename);

#endif // _FILES_H_