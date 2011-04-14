
#ifndef PQ_H
#define PQ_H

/* a priority queue, lower values in front */
typedef struct _PQ *PQ;

PQ new_queue(int capacity);
void free_queue(PQ q);

/* modifying the queue */
int queue_insert(PQ q, double priority, void *data); /* returns 0 if the queue could not be resized */
void *remove_queue_min(PQ q);
void remove_queue_items(PQ q, void *data); /* removes all entries with this data */

/* querying the queue */
void *queue_min(PQ q);
double queue_min_priority(PQ q); /* returns -1 if no items */
int queue_empty(PQ q);
int queue_count(PQ q);

#endif
