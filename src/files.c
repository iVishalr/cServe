#include <stdio.h>
#include <stdlib.h>
#include <sys/stat.h>
#include "files.h"
#include "utils.h"
#include <math.h>
#define STB_IMAGE_IMPLEMENTATION
#include "stb_image.h"

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
    buffer = (char*)malloc((bytes_remaining+1));
    ptr = buffer;

    if (buffer == NULL){
        return NULL;
    }
    printf("Reading file contents.\n");
    
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

    file_data * filedata = (file_data*)malloc(sizeof(file_data));
    printf("allocated filedata struct.\n");

    if (filedata == NULL){
        free(buffer);
        buffer = NULL;
        return NULL;
    }

    filedata->data = buffer;
    filedata->size = strlen(buffer); // to account for '\0'
    filedata->filename = filename;
    return filedata;
}

// file_data *read_image(char *filename){

//     if (filename == NULL){
//         fprintf(stderr, "Please provide a valid filename.\n");
//         return NULL;
//     }
    
//     int height, width, bpp;
//     void *image = stbi_load(filename, &width, &height, &bpp, 3);

//     if (image == NULL){
//         fprintf(stderr, "Error while loading image from %s.\n", filename);
//         return NULL;
//     }
//     // printf("Size of %s is : %ld bytes. strlen(%s)=%ld. HxCxW=%d\n", filename ,sizeof(image), filename, strlen(image), height * width * bpp);
//     file_data *filedata = (file_data*)malloc(sizeof(file_data));

//     if (filedata == NULL){
//         fprintf(stderr, "Error while allocating memory to file_data struct.\n");
//         return NULL;
//     }

//     filedata->data = image;
//     filedata->filename = filename;
//     filedata->size = strlen(image)-1;
//     return filedata;
// }

// int read_image_file(char *file_name, char **buffer, int *sizeof_buffer) {
//     int c, i;
//     int char_read = 0;
//     FILE* fp = fopen(file_name, "rb");

//     if (fp == NULL) {
//             fprintf(stderr, "\t Can't open file : %s", file_name);
//             return -1;
//     }
    
//     fseek(fp, 0L, SEEK_END);
//     *sizeof_buffer = ftell(fp);
//     fseek(fp, 0L, SEEK_SET);
//     *buffer = (char *)malloc(*sizeof_buffer);
//     fread(*buffer, *sizeof_buffer, 1, fp);
//     return 0;
// }

file_data *read_image(char *filename){

    if (filename == NULL){
        fprintf(stderr, "Please provide a valid filename.\n");
        return NULL;
    }
    
    FILE *fp = fopen(filename, "rb");
    if (fp == NULL){
        fprintf(stderr, "Could not open file %s.\n", filename);
        return NULL;
    }
    fseek(fp, 0L, SEEK_END);
    long size = ftell(fp);
    rewind(fp);
    long int bytes_read, bytes_remaining, total_bytes = 0;
    bytes_remaining = size;
    void *buffer = malloc((size));
    char *ptr = buffer;

    if (buffer == NULL){
        return NULL;
    }
    printf("Reading file contents.\n");
    
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

    // buffer[total_bytes] = '\0';
    fclose(fp);
    
    file_data *filedata = (file_data*)malloc(sizeof(file_data));
    if (filedata == NULL){
        fprintf(stderr, "Error while allocating memory to file_data struct.\n");
        return NULL;
    }

    printf("\nSIZE = %ld | Bytes received = %ld | sizeof image = %ld\n", size, total_bytes, strlen(buffer));

    filedata->data = buffer;
    filedata->filename = filename;
    filedata->size = total_bytes;
    return filedata;
}

// void free_image(uint8_t *image){
//     if (image == NULL) return;
//     // stbi_image_free(image);
//     image = NULL;
// }

void file_free(file_data *filedata){
    if (filedata == NULL) return;
    if (filedata->data)
        free(filedata->data);
    free(filedata);
    filedata = NULL;
}