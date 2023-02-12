#include <stdio.h>
#include <stdlib.h>
#include <sys/stat.h>
#include "files.h"
#include "utils.h"

file_data *file_load(char *filename){

    char *buffer, *ptr;
    struct stat buf;
    long int bytes_read, bytes_remaining, total_bytes = 0;

    // obtain the file size of the filename passed
    if (stat(filename, &buf) == -1){
        printf("cannot load file size!\n");
        return NULL;
    }

    // check if the file is a regular file
    if (!(buf.st_mode & S_IFREG)){
        printf("not a regular file!\n");
        return NULL;
    }

    // open file for reading
    FILE *fp = fopen(filename, "rb");

    if (fp==NULL){
        printf("cannot open file!\n");
        return NULL;
    }

    bytes_remaining = buf.st_size;
    ptr = buffer = (char*)malloc((bytes_remaining+1));

    if (buffer == NULL){
        return NULL;
    }

    while (
        bytes_read = fread(ptr, 1, bytes_remaining, fp), 
        bytes_read != 0 && bytes_remaining > 0
    ){
        if (bytes_read == -1){
            free(buffer);
            buffer = NULL;
            return NULL;
        }

        bytes_remaining = bytes_remaining - bytes_read;
        ptr = ptr + bytes_read;
        total_bytes = total_bytes + bytes_read;
    }

    buffer[total_bytes] = '\0';
    fclose(fp);

    file_data * filedata = (file_data*)malloc(sizeof(*filedata));

    if (filedata == NULL){
        free(buffer);
        buffer = NULL;
        return NULL;
    }

    filedata->data = buffer;
    filedata->size = total_bytes + 1; // to account for '\0'
    filedata->filename = filename;
    return filedata;
}

void file_free(file_data *filedata){
    free(filedata->data);
    free(filedata);
    filedata = NULL;
}