#include <stdio.h>
#include <stdlib.h>
#include <pthread.h>
#include "queues.h"

queues *queue_create()
{
    queues *queue = (queues *)malloc(sizeof(queues));

    if (queue == NULL)
    {
        fprintf(stderr, "Error allocating memory to queue.\n");
        return NULL;
    }

    queue->head = NULL;
    queue->tail = NULL;
    queue->size = 0;
    queue->mutex = (pthread_mutex_t)PTHREAD_MUTEX_INITIALIZER;
    queue->condition_var = (pthread_cond_t)PTHREAD_COND_INITIALIZER;
    return queue;
}

queue_node *queue_create_node(void *data)
{
    queue_node *node = (queue_node *)malloc(sizeof(queue_node));

    if (node == NULL)
    {
        fprintf(stderr, "Error allocating memory to node.\n");
        return NULL;
    }

    node->data = data;
    node->next = NULL;
    node->prev = NULL;
    return node;
}

void enqueue_critical_section(queues *queue, queue_node *node)
{

    if (queue->tail == NULL && queue->head == NULL)
    {
        queue->head = node;
        queue->tail = node;
    }
    else
    {
        node->prev = queue->tail;
        queue->tail->next = node;
        queue->tail = node;
    }
    queue->size += 1;
}

void *enqueue(queues *queue, void *data)
{
    if (queue == NULL || data == NULL)
    {
        return NULL;
    }

    queue_node *node = queue_create_node(data);

    if (node == NULL)
    {
        fprintf(stderr, "Error while creating new node.\n");
        return NULL;
    }
    pthread_mutex_lock(&queue->mutex);
    enqueue_critical_section(queue, node);
    pthread_mutex_unlock(&queue->mutex);
    return data;
}

queue_node *dequeue_critical_section(queues *queue)
{
    queue_node *node = NULL;
    if (queue->head == NULL)
    {
        return NULL;
    }
    if (queue->head == queue->tail)
    {
        node = queue->head;
        queue->head = NULL;
        queue->tail = NULL;
    }
    else
    {
        node = queue->head;
        queue->head = node->next;
        queue->head->prev = NULL;
    }
    queue->size -= 1;
    return node;
}

void *dequeue(queues *queue)
{
    if (queue == NULL)
    {
        return NULL;
    }

    void *data = NULL;
    pthread_mutex_lock(&queue->mutex);
    queue_node *node = dequeue_critical_section(queue);
    pthread_mutex_unlock(&queue->mutex);
    if (node == NULL)
    {
        return NULL;
    }

    data = node->data;
    node->prev = NULL;
    node->next = NULL;
    free(node);
    node = NULL;
    return data;
}

void queue_destroy(queues *queue)
{
    if (queue == NULL)
    {
        return;
    }

    queue_node *node = queue->head, *temp = NULL;
    while (node != NULL)
    {
        temp = node;
        node = node->next;
        free(temp);
        temp = NULL;
    }
    node = NULL;
    free(queue);
    queue = NULL;
}