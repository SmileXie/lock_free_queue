#ifndef _LF_QUEUE_H
#define _LF_QUEUE_H

#include <stdint.h>
#include <stdlib.h>
#include <stdio.h>
#include <stdbool.h>
#include <inttypes.h>

#define LF_QUEUE_DATA_LEN   (512)

struct lf_queue_node {
    struct lf_queue_node *next;
    uintptr_t aba;
    char data[LF_QUEUE_DATA_LEN];
};

/* queue head and tail */
struct lf_queue_head {
    uintptr_t aba;
    struct lf_queue_node *node;
};

typedef struct {
    _Atomic struct lf_queue_head head, tail;
    _Atomic size_t size;
} queue_t;

int lf_queue_init(queue_t *queue);

int lf_queue_enqueue(queue_t *queue, char *data, size_t data_len);

int lf_queue_dequeue(queue_t *queue, char *data, size_t data_len);

#endif /* #ifndef _LF_QUEUE_H */