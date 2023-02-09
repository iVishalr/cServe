#ifndef _ROUTES_H_
#define _ROUTES_H_

typedef struct route_node{
    char *key;
    char *value;
    struct route_node *left, *right;
} route_node;

typedef struct route_map{
    route_node * map;
    int num_routes;
} route_map;

extern route_map *create();
extern void *register_route(route_map *map, char *key, char *value);
extern route_node *search(route_map *map, char *key);
extern void *delete_route(route_map *map, char *key);
extern void inorder_traversal(route_map *map);
extern void destroy(route_map *map);

#endif // _ROUTES_H_