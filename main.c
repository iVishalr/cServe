#include <stdio.h>
#include "server.h"
#include "files.h"
#include "mime.h"
#include <string.h>

void custom_fn(void *server, int new_socket_fd, const char *path, void *args)
{
    http_server *s = (http_server *)server;
    char filepath[4096];
    sprintf(filepath, "%s/bio.html", path);
    file_data *filedata = file_load(filepath);
    char *mime_type = mime_type_get(filepath);
    if (filedata != NULL && mime_type != NULL)
    {
        send_http_response(
            server, new_socket_fd, HEADER_OK, mime_type, filedata->data, filedata->size);
    }
    else
    {
        char body[] = "Resource not Found!";
        send_http_response(server, new_socket_fd, HEADER_404, "text/html", body, strlen(body));
    }
}

int main()
{
    size_t method_len = 1;
    char *methods[1] = {"GET"};
    http_server *server = create_server(8080, 0, 0, "static-website-example", 0, 0, 1000);
    server_route(server, "/", "index.html", methods, method_len, NULL, NULL, NULL);
    server_route(server, "/about", NULL, methods, method_len, "/", custom_fn, NULL);
    printf("Starting server\n");
    server_start(server, 1, 1);
    return 0;
}