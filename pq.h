
#ifndef PQ_H
#define PQ_H

typedef struct PQ *PQ;

PQ new_queue(int capacity);
void *queue_min(PQ q);
void *remove_queue_min(PQ q);
int queue_insert(PQ q, double priority, void *data); /* returns 0 if the queue could not be resized */

#endif
