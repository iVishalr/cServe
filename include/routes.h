#ifndef _ROUTES_H_
#define _ROUTES_H_

typedef struct route_node{
    const char *key;
    const char *value;
    struct route_node *left, *right;
} route_node;

typedef struct route_map{
    route_node * map;
    int num_routes;
} route_map;

extern route_map *route_create();
extern void *register_route(route_map *map, const char *key, const char *value);
extern route_node *route_search(route_map *map, const char *key);
extern void *route_delete(route_map *map, const char *key);
extern void route_inorder_traversal(route_map *map);
extern void route_destroy(route_map *map);

#endif // _ROUTES_H_