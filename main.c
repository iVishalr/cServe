#include <stdio.h>
#include "server.h"

int main(){
    http_server *server = create_server(
        8080, 128, 128, "./static", 1024, 1024
    );

    print_server_logs(server);
    destroy_server(server, 1);
    return 0;
}