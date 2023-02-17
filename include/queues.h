#ifndef _QUEUES_H_
#define _QUEUES_H_

#ifdef __cplusplus
extern "C"
{
#endif
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
        pthread_cond_t condition_var;
    } queues;
    queues *queue_create();
    queue_node *queue_create_node(void *data);
    void *enqueue(queues *queue_ptr, void *data);
    void *dequeue(queues *queue_ptr);
    void queue_destroy(queues *queue_ptr);
#ifdef __cplusplus
}
#endif

#endif //_QUEUES_H_