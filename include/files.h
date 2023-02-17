#ifndef _FILES_H_
#define _FILES_H_

#ifdef __cplusplus
extern "C"
{
#endif
    typedef struct file_data
    {
        int size;
        void *data;
        char *filename;
    } file_data;
    file_data *file_load(char *filename);
    void file_free(file_data *filedata);
    int get_image_fd(char *filename);
    file_data *read_image(char *filename);
#ifdef __cplusplus
}
#endif

#endif // _FILES_H_