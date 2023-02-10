#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "routes.h"

route_map *route_create(){
    route_map *map = (route_map*)malloc(sizeof(*map));

    if (map == NULL){
        return NULL;
    }

    map->map = NULL;
    map->num_routes = 0;
    return map;
}

route_node *create_node(const char *key, const char *value){
    route_node *node = (route_node*)malloc(sizeof(*node));

    if (node == NULL){
        return NULL;
    }

    node->key = key;
    node->value = value;
    node->left = NULL;
    node->right = NULL;
    return node;
}

route_node *register_route_handler(route_node *root, const char *key, const char *value){
    if (root == NULL){
        return create_node(key, value);
    }

    if (strcmp(key, root->key) == 0){
        fprintf(stderr, "WARN: Route %s already exists! Hence ignored.\n", key);
    }
    else if (strcmp(key, root->key) < 0){
        root->left = register_route_handler(root->left, key, value);
    }
    else{
        root->right = register_route_handler(root->right, key, value);
    }
    return NULL;
}

void *register_route(route_map *map, const char *key, const char *value){
    route_node *root = map->map;

    if (root == NULL){
        map->map = register_route_handler(map->map, key, value);
    }
    else{
        register_route_handler(map->map, key, value);
    }

    map->num_routes+=1;
}

route_node *search_handler(route_node *root, const char *key){
    if (root == NULL){
        return NULL;
    }

    if (strcmp(key, root->key)==0){
        return root;
    }
    else if (strcmp(key, root->key)<0){
        return search_handler(root->left, key);
    }
    else{
        return search_handler(root->right, key);
    }
}

route_node *route_search(route_map *map, const char *key){
    return search_handler(map->map, key);
}

/*
 * Removes the node from bst
 * Cannot delete root node
*/
route_node *delete_handler(route_node *node, route_node *parent, const char *key){
    int diff = strcmp(key,node->key);
    if (node == NULL || (diff == 0 && parent == NULL)){
        return NULL;
    }

    if (diff == 0){
        // found the node to delete
        // cleanup parent and return node 
        if (node == parent->left){
            parent->left = NULL;
        }
        else{
            parent->right = NULL;
        }
        return node;
    }
    else if (diff < 0){
        return delete_handler(node->left, node, key);
    }
    else{
        return delete_handler(node->right, node, key);
    }
    return NULL;
}

void free_route_node(route_node *node){
    free(node);
    node = NULL;
}

void *route_delete(route_map *map, const char *key){
    route_node *delNode = delete_handler(map->map, NULL, key);
    if (delNode == NULL){
        fprintf(stderr, "Could not delete route %s. Either route \'%s\' is not found or you are deleting the root of route_map.\n", key, key);
    }
    else{
        free_route_node(delNode);
        delNode = NULL;
    }
    map->num_routes -= 1;
    return NULL;
}

void inorder_traversal_handler(route_node *root){
    if (root == NULL){
        return;
    }

    inorder_traversal_handler(root->left);
    fprintf(stdout, "Node(key = %s, value = %s)\n",root->key, root->value);
    inorder_traversal_handler(root->right);
}

void route_inorder_traversal(route_map *map){
    if (map->map == NULL){
        fprintf(stdout, "No routes to display.\n");
        return;
    }
    inorder_traversal_handler(map->map);
}

void destroy_route_handler(route_node *root){
    if (root == NULL){
        return;
    }

    destroy_route_handler(root->right);
    destroy_route_handler(root->left);
    free_route_node(root);
    root = NULL;
}

void route_destroy(route_map *map){
    destroy_route_handler(map->map);
    map->map = NULL;
    free(map);
    map = NULL;
}