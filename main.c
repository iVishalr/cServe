#include <stdio.h>
#include "server.h"
#include "files.h"
#include "mime.h"

void custom_fn(void * server, int new_socket_fd, const char *path, void *args){
    http_server *s = (http_server*)server;
    char filepath[4096];
    sprintf(filepath, "%s/index.html", path);
    file_data * filedata = file_load(filepath);
    char *mime_type = mime_type_get(filepath);
    if (filedata != NULL && mime_type != NULL){
        send_http_response(
            server, new_socket_fd, HEADER_OK, mime_type, filedata->data
        );
    }else{
        send_http_response(server, new_socket_fd, HEADER_404, "text/html", "Resource not Found!");
    }
}

int main(){
    size_t method_len = 3;
    char *methods[3] = {"GET", "POST", "DELETE"};
    http_server *server = create_server(8082, 8,8, "static-website-example", 0, 0, 1000);
    register_route(server->route_table, "/", "index.html", methods, method_len, NULL, NULL, NULL);
    server_start(server, 1, 1);
    return 0;
}