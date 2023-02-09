#include <string.h>
#include <ctype.h>
#include "mime.h"

#define DEFAULT_MIME_TYPE "application/octet-stream"

char *strlower(char *string){
    for (char *p=string; *p!='\0'; p++){
        *p = tolower(*p);
    }
    return string;
}

char *mime_type_get(char *filename){
    char *extension = strrchr(filename, '.'); // returns ptr to the last occurence of '.'
    if (extension==NULL){
        return DEFAULT_MIME_TYPE;
    }

    extension++; //increment to point to the first char of file extension
    extension = strlower(extension);
    
    if (strcmp(extension, "html") == 0 || strcmp(extension, "htm") == 0) { return "text/html"; }
    if (strcmp(extension, "jpeg") == 0 || strcmp(extension, "jpg") == 0) { return "image/jpg"; }
    if (strcmp(extension, "css") == 0) { return "text/css"; }
    if (strcmp(extension, "js") == 0) { return "application/javascript"; }
    if (strcmp(extension, "json") == 0) { return "application/json"; }
    if (strcmp(extension, "txt") == 0) { return "text/plain"; }
    if (strcmp(extension, "gif") == 0) { return "image/gif"; }
    if (strcmp(extension, "png") == 0) { return "image/png"; }

    return DEFAULT_MIME_TYPE;
}