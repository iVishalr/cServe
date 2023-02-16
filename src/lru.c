#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "lru.h"

cache_node *allocate_node(char *key, char *content_type, void *content, int content_length)
{
    cache_node *node = (cache_node *)malloc(sizeof(cache_node));

    if (node == NULL)
    {
        return NULL;
    }

    long key_size = strlen(key);
    node->key = (char *)malloc(key_size + 1);
    sprintf(node->key, "%s", key);
    node->content_type = content_type;
    node->content = content;
    node->content_length = content_length;
    node->next = NULL;
    node->prev = NULL;
    return node;
}

void free_cache_node(cache_node *node)
{
    if (node == NULL)
    {
        return;
    }
    if (node->key)
    {
        free(node->key);
    }
    if (node->content)
    {
        free(node->content); // dont miss this :)
    }
    free(node);
    node = NULL;
}

void insert_at_head(lru *lru_cache, cache_node *node)
{
    if (node == NULL || lru_cache == NULL)
    {
        return;
    }

    if (lru_cache->head == NULL)
    {
        lru_cache->head = node;
        lru_cache->tail = node;
    }
    else
    {
        node->next = lru_cache->head;
        lru_cache->head->prev = node;
        lru_cache->head = node;
    }
    lru_cache->current_size++;
}

void move_to_head(lru *lru_cache, cache_node *node)
{
    if (node == NULL || lru_cache == NULL)
    {
        return;
    }

    // check if node is currently at head position
    if (node == lru_cache->head)
    {
        return;
    }

    // check if node is currently at tail position
    if (node == lru_cache->tail)
    {
        lru_cache->tail = node->prev;
        node->prev->next = NULL;
    }
    else
    {
        // if node is in middle of DLL,
        node->prev->next = node->next;
        node->next->prev = node->prev;
    }
    lru_cache->current_size--; // subtract 1 from current size to cancel out addition at insert
    // we have detached node from DLL. Attach to head
    insert_at_head(lru_cache, node);
}

cache_node *remove_tail(lru *lru_cache)
{
    cache_node *node = lru_cache->tail;

    lru_cache->tail = node->prev;
    node->prev->next = NULL;
    lru_cache->current_size--;
    // clean up node before returning
    node->prev = NULL;
    node->next = NULL;
    return node;
}

lru *lru_create(int max_size, int hashsize)
{
    lru *lru_cache = (lru *)malloc(sizeof(lru));
    lru_cache->head = NULL;
    lru_cache->tail = NULL;
    lru_cache->current_size = 0;
    lru_cache->max_size = max_size;
    lru_cache->table = hashtable_create(hashsize, NULL);

    if (lru_cache->table == NULL)
    {
        fprintf(stderr, "Error creating hashtable for LRU cache.\n");
        exit(1);
    }
    return lru_cache;
}

void destroy_cache(lru *lru_cache)
{
    if (lru_cache == NULL)
    {
        return;
    }
    cache_node *node = lru_cache->head;
    cache_node *next = NULL;
    while (node != NULL)
    {
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

cache_node *cache_put(lru *lru_cache, char *key, char *content_type, void *content, int content_length)
{
    if (lru_cache == NULL)
    {
        return NULL;
    }

    if (key == NULL)
    {
        fprintf(stderr, "Key is a required argument.\n");
        return NULL;
    }

    if (content == NULL)
    {
        fprintf(stderr, "No data provided for key=%s. Key not added to cache.\n", key);
        return NULL;
    }
    // create a cache node
    cache_node *node = allocate_node(key, content_type, content, content_length);

    if (node == NULL)
    {
        fprintf(stderr, "An error occured when allocating memory to cache_node.\n");
        return NULL;
    }

    // check if the cache is full
    if (lru_cache->current_size == lru_cache->max_size)
    {
        // cache is full. Evict a node from tail of cache
        cache_node *lru_node = remove_tail(lru_cache);
        // remove the node from hashtable
        hashtable_delete(lru_cache->table, lru_node->key);
        free_cache_node(lru_node);
        lru_node = NULL;
    }

    // add the node to MRU side of linked list
    insert_at_head(lru_cache, node);
    // add the node to hashtable for O(1) access to the node
    hashtable_put(lru_cache->table, key, node);
    return node;
}

cache_node *cache_get(lru *lru_cache, char *key)
{
    cache_node *node = hashtable_get(lru_cache->table, key);
    if (node == NULL)
    {
        return NULL;
    }

    // move the node's position in DLL to head
    move_to_head(lru_cache, node);
    return node;
}

void cache_print(lru *lru_cache)
{
    if (lru_cache == NULL)
    {
        return;
    }

    cache_node *node = lru_cache->head;
    int index = 0;
    while (node != NULL)
    {
        printf("Index: %d Key=%s ContentLength=%d\n", index, node->key, node->content_length);
        index += 1;
        node = node->next;
    }
}