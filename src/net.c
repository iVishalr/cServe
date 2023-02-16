#include <stdio.h>
#include <unistd.h>
#include <string.h>
#include <netdb.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <sys/types.h>
#include <sys/socket.h>
#include "net.h"

#define NUM_PENDING_CONNECTIONS 10

/**
 * Function to get Internet address, either as IPv4 or IPv6
 * Helper function to make printing easier.
 */
void *get_internet_address(struct sockaddr *socket_addr)
{
    if (socket_addr->sa_family == AF_INET)
    {
        // if internet address is IPv4
        return &(((struct sockaddr_in *)socket_addr)->sin_addr);
    }
    else
    {
        return &(((struct sockaddr_in6 *)socket_addr)->sin6_addr);
    }
}

/**
 * Returns the main listening socket
 * Returns -1 or error
 */
int get_listener_socket(char *port, int backlog)
{
    int socket_fd;
    struct addrinfo hints, *servinfo, *p;
    int yes = 1;
    int rv;

    memset(&hints, 0, sizeof(hints));
    hints.ai_family = AF_UNSPEC;
    hints.ai_socktype = SOCK_STREAM;
    hints.ai_flags = AI_PASSIVE; // use host IP

    rv = getaddrinfo(NULL, port, &hints, &servinfo);
    if (rv != 0)
    {
        fprintf(stderr, "%s: %d getaddrinfo: %s\n", __FILE__, __LINE__, gai_strerror(rv));
        return -1;
    }

    // servinfo stores list of potential interfaces.
    // loop through the interfaces, and set up socket on each.
    // Stop looping if a socket is setup successfully on any one of the interfaces
    for (p = servinfo; p != NULL; p = p->ai_next)
    {
        // Try to create a socket based on the current candidate interface
        socket_fd = socket(p->ai_family, p->ai_socktype, p->ai_protocol);
        if (socket_fd == -1)
        {
            perror("serve: socket");
            continue;
        }

        // check if address is already in use
        if (
            setsockopt(socket_fd, SOL_SOCKET, SO_REUSEADDR, &yes, sizeof(int)) == -1)
        {
            perror("setsockopt");
            close(socket_fd);
            freeaddrinfo(servinfo);
            return -2;
        }

        // check if we can bind the socket to this local IP address.
        if (bind(socket_fd, p->ai_addr, p->ai_addrlen) == -1)
        {
            close(socket_fd);
            perror("server: bind");
            continue;
        }

        // at this point we have bounded a socket to the given port
        break;
    }

    freeaddrinfo(servinfo);
    // check if p is NULL. Which means that we could not find any interface to setup a socket
    if (p == NULL)
    {
        fprintf(stderr, "server: failed to find local address\n");
        return -3;
    }

    // start listening using the socket file descriptor
    fprintf(stdout, "Server listening on port %s...\n", port);
    if (listen(socket_fd, backlog) == -1)
    {
        perror("server: Failed to listen");
        close(socket_fd);
        return -4;
    }

    return socket_fd;
}