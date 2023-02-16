#ifndef _QUEUES_H_
#define _QUEUES_H_

typedef struct queue_node
{
    void *data;
    struct queue_node *next;
    struct queue_node *prev;
} queue_node;

typedef struct queues
{
    queue_node *head;
    queue_node *tail;
    int size;
    pthread_mutex_t mutex;
} queues;

extern queues *queue_create();
extern queue_node *queue_create_node(void *data);
extern void *enqueue(queues *queue_ptr, void *data);
extern void *dequeue(queues *queue_ptr);
extern void queue_destroy(queues *queue_ptr);

#endif //_QUEUES_H_