
#ifndef PQ_H
#define PQ_H

/* a minimum priority queue */
typedef struct PQ *PQ;

PQ new_queue(int capacity);
void free_queue(PQ q);
int queue_insert(PQ q, double priority, void *data); /* returns 0 if the queue could not be resized */
void *queue_min(PQ q);
void *remove_queue_min(PQ q);
void remove_queue_items(PQ q, void *data); /* removes all entries with this data */
int queue_empty(PQ q);

#endif
