#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "lru.h"

cache_node *allocate_node(char *path, char *content_type, void *content, int content_length){
    cache_node *node = (cache_node*)malloc(sizeof(*node));

    if (node == NULL){
        return NULL;
    }

    node->path = path;
    node->content_type = content_type;
    node->content = content;
    node->content_length = content_length;
    node->next = NULL;
    node->prev = NULL;
    return node;
}

void free_cache_node(cache_node *node){
    if (node == NULL){
        return NULL;
    }

    free(node);
    node = NULL;
}

void insert_at_head(lru *lru_cache, cache_node *node){
    if (node == NULL){
        return;
    }

    if (lru_cache->head == NULL){
        lru_cache->head = node;
        lru_cache->tail = node;
    }
    else{
        node->next = lru_cache->head;
        lru_cache->head->prev = node;
        lru_cache->head = node;
    }
    lru_cache->current_size++;
}

void move_to_head(lru *lru_cache, cache_node *node){
    if (node == NULL){
        return NULL;
    }

    // check if node is currently at head position
    if (node == lru_cache->head){
        return NULL;
    }

    // check if node is currently at tail position
    if (node == lru_cache->tail){
        lru_cache->tail = node->prev;
        node->prev->next = NULL;
    }
    else{
        // if node is in middle of DLL,
        node->prev->next = node->next;
        node->next->prev = node->prev;
    }
    lru_cache->current_size--; // subtract 1 from current size to cancel out addition at insert
    // we have detached node from DLL. Attach to head
    insert_at_head(lru_cache, node);
}

cache_node *remove_tail(lru *lru_cache){
    cache_node *node = lru_cache->tail;

    lru_cache->tail = node->prev;
    node->prev->next = NULL;
    lru_cache->current_size--;
    //clean up node before returning 
    node->prev = NULL;
    node->next = NULL;
    return node;
}

lru *lru_create(int max_size, int hashsize){
    lru *lru_cache = (lru*)malloc(sizeof(*lru_cache));
    lru_cache->head = NULL;
    lru_cache->tail = NULL;
    lru_cache->current_size = 0;
    lru_cache->max_size = max_size;
    lru_cache->table = hashtable_create(hashsize, NULL);
    return lru_cache;
}

void cache_free(lru *lru_cache){
    cache_node *node = lru_cache->head;
    cache_node *next = NULL;
    while (node!=NULL){
        next = node->next;
        free_cache_node(node);
        lru_cache->current_size--;
        node = next;
    }
    // destroy hashtable
    hashtable_destroy(lru_cache->table);
    lru_cache->head = NULL;
    lru_cache->tail = NULL;
    lru_cache->table = NULL;
    free(lru_cache);
    lru_cache = NULL;
}

void cache_put(lru *lru_cache, char *path, char *content_type, void *content, int content_length){
    // create a cache node
    cache_node *node = allocate_node(path, content_type, content, content_length);

    //check if the cache is full 
    if (lru_cache->current_size == lru_cache->max_size){
        // cache is full. Evict a node from tail of cache
        cache_node *lru_node = remove_tail(lru_cache);
        // remove the node from hashtable
        hashtable_delete(lru_cache->table, lru_node->path);
        free_cache_node(lru_node);
        lru_node = NULL;
    }
    // add the node to MRU side of linked list
    insert_at_head(lru_cache, node);

    // add the node to hashtable for O(1) access to the node
    hashtable_put(lru_cache->table, path, node);
}

cache_node *cache_get(lru *lru_cache, char *path){
    cache_node *node = hashtable_get(lru_cache->table, path);
    if (node == NULL){
        return NULL;
    }
    
    // move the node's position in DLL to head
    move_to_head(lru_cache, node);
    return node;
}