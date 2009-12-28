
#include "pq.h"

#include <stdlib.h>

typedef struct {
	double priority;
	void *data;
	int floats; /* if true, always swims to top */
} PQItem;

struct PQ {
	int count, capacity;
	PQItem *items; /* stored starting at index 1 */
};

static void exchange(PQ q, int i, int j);
static void swim(PQ q, int k);
static void sink(PQ q, int k);

PQ
new_queue(int capacity)
{
	PQ q = malloc(sizeof(PQ));
	
	if(q == NULL)
		return NULL;
	
	q->items = malloc(sizeof(PQItem) * (capacity + 1));
	
	if(q->items == NULL) {
		free(q);
		return NULL;
	}
	
	q->count = 0;
	q->capacity = capacity;
	
	return q;
}

void
free_queue(PQ q)
{
	if(q) {
		free(q->items);
		free(q);
	}
}

/* returns 0 if the queue is full and could not be resized */
int
queue_insert(PQ q, double priority, void *data)
{
	int i;
	
	if(q->count == q->capacity) {
		int new_capacity = q->capacity == 0 ? 1 : q->capacity * 2;
		
		PQItem *new_items = malloc(sizeof(PQItem) * (new_capacity + 1));
		
		if(new_items == NULL)
			return 0;
		
		for(i = 1; i <= q->count; i++)
			new_items[i] = q->items[i];
		
		free(q->items);
		
		q->items = new_items;
		q->capacity = new_capacity;
	}
	
	q->items[++q->count].priority = priority;
	q->items[q->count].data = data;
	q->items[q->count].floats = 0;
	swim(q, q->count);
	
	return 1;
}

void *
queue_min(PQ q)
{
	if(q->count > 0)
		return q->items[1].data;
	
	return NULL;
}

void *
remove_queue_min(PQ q)
{
	void *data;
	
	if(q->count == 0)
		return NULL;
	
	data = q->items[1].data;
	
	exchange(q, 1, q->count--);
	sink(q, 1);
	
	return data;
}

void
remove_queue_items(PQ q, void *data)
{
	int i;
	
	for(i = q->count; i >= 1; i--) {
		if(q->items[i].data == data) {
			q->items[i].floats = 1;
			swim(q, i);
			remove_queue_min(q);
		}
	}
}

int
queue_empty(PQ q)
{
	return q->count == 0;
}

/* PRIVATE HELPERS */

#define MORE(i, j) (q->items[j].floats || (q->items[i].priority > q->items[j].priority))

static
void
exchange(PQ q, int i, int j)
{
	PQItem tmp = q->items[i];
	q->items[i] = q->items[j];
	q->items[j] = tmp;
}

static
void
swim(PQ q, int k)
{
	while(k > 1 && MORE(k/2, k)) {
		exchange(q, k, k/2);
		k /= 2;
	}
}

static
void
sink(PQ q, int k)
{
	int j;
	
	while(2*k <= q->count) {
		j = 2 * k;
		
		if(j < q->count && MORE(j, j+1))
			j++;
		
		if(!MORE(k, j))
			break;
		
		exchange(q, k, j);
		k = j;
	}
}