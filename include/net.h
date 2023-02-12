#ifndef _NET_H_
#define _NET_H_

void *get_internet_address(struct sockaddr *socket_addr);
int get_listener_socket(char *port, int backlog);

#endif