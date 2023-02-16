#include <stdio.h>
#include <stdlib.h>
#include <pthread.h>
#include "queues.h"

queues *queue_create()
{
    queues *queue = (queues *)malloc(sizeof(queue));

    if (queue == NULL)
    {
        fprintf(stderr, "Error allocating memory to queue.\n");
        return NULL;
    }

    queue->head = NULL;
    queue->tail = NULL;
    queue->size = 0;
    queue->mutex = (pthread_mutex_t)PTHREAD_MUTEX_INITIALIZER;
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
    // pthread_mutex_lock(&queue->mutex);
    if (queue == NULL || data == NULL)
    {
        return NULL;
    }

    printf("Creating new queue node\n");
    queue_node *node = queue_create_node(data);

    if (node == NULL)
    {
        fprintf(stderr, "Error while creating new node.\n");
        return NULL;
    }
    printf("Entering critical section : ENQUEUE\n");
    enqueue_critical_section(queue, node);
    printf("Done critical section : ENQUEUE\n");
    // pthread_mutex_unlock(&queue->mutex);
    return data;
}

queue_node *dequeue_critical_section(queues *queue)
{
    queue_node *node = NULL;
    if (queue->head == queue->tail)
    {
        printf("HEAD == TAIL\n");
        node = queue->head;
        queue->head = NULL;
        queue->tail = NULL;
    }
    else
    {
        printf("HEAD != TAIL\n");
        node = queue->head;
        queue->head = node->next;
        queue->head->prev = NULL;
    }
    queue->size -= 1;
    return node;
}

void *dequeue(queues *queue)
{
    // pthread_mutex_lock(&queue->mutex);

    if (queue == NULL || queue->head == NULL)
    {
        return NULL;
    }

    printf("In dequeue\n");
    void *data = NULL;
    printf("Entering critical section : DEQUEUE\n");
    queue_node *node = dequeue_critical_section(queue);
    printf("Done critical section : DEQUEUE\n");

    if (node == NULL)
    {
        return NULL;
    }

    if (queue->head == NULL)
    {
        printf("\nHEAD == NULL\n");
    }

    if (queue->tail == NULL)
    {
        printf("TAIL == NULL\n");
    }

    // pthread_mutex_unlock(&queue->mutex);

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