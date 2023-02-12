#ifndef _ROUTES_H_
#define _ROUTES_H_

typedef struct route_node{
    const char *key;
    const char *value;
    const char *route_dir;
    void (*route_fn)(void *, int, const char *, void *);
    void *fn_args;
    char **methods;
    int num_methods;
    struct route_node *left, *right;
} route_node;

typedef struct route_map{
    route_node * map;
    int num_routes;
} route_map;

extern route_map *route_create();
extern void *register_route(route_map *map, const char *key, const char *value, char **methods, size_t method_len, const char *route_dir, void (*route_fn)(void *, int, const char *, void *), void *fn_args);
extern route_node *route_search(route_map *map, const char *key);
extern void *route_delete(route_map *map, const char *key);
extern void route_inorder_traversal(route_map *map);
extern void route_destroy(route_map *map);
extern void route_node_print(route_node *node);
extern int route_check_method(route_node *node, char *method);

#endif // _ROUTES_H_