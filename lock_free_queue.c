#include <string.h>
#include <pthread.h>
#include <stdatomic.h>
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

queue_t lf_queue;

#define LFQ_LOG_ERROR printf
#define LFQ_LOG_INFO printf

int lf_queue_init(queue_t *queue)
{
    struct lf_queue_head head_init;
    struct lf_queue_node *init_node;

    if (queue == NULL) {
        LFQ_LOG_ERROR("queue is NULL\n");
        return -1;
    }
    
    init_node = (struct lf_queue_node *)malloc(sizeof(struct lf_queue_node));
    if (init_node == NULL) {
        LFQ_LOG_ERROR("fail to malloc.\n");
        return -1;
    }

    memset(init_node, 0, sizeof(struct lf_queue_node));
    init_node->next = NULL;
    init_node->data[0] = 111;

    head_init.aba = 0;
    head_init.node = init_node;

    queue->head = ATOMIC_VAR_INIT(head_init);
    queue->tail = ATOMIC_VAR_INIT(head_init);
    queue->size = ATOMIC_VAR_INIT(0);

    LFQ_LOG_INFO("queue %p is initialized", queue);

    return 0;
}

int lf_queue_enqueue(queue_t *queue, char *data, size_t data_len)
{
    struct lf_queue_node *new_node, *null_node = NULL;
    struct lf_queue_head new_tail, tmp_tail;

    if (queue == NULL) {
        LFQ_LOG_ERROR("queue is NULL\n");
        return -1;
    }

    if (data_len > LF_QUEUE_DATA_LEN) {
        LFQ_LOG_ERROR("datalen is %lu, max length is %d\n", data_len, LF_QUEUE_DATA_LEN);
        return -1;
    }

    new_node = (struct lf_queue_node *)malloc(sizeof(struct lf_queue_node));
    if (new_node == NULL) {
        LFQ_LOG_ERROR("fail to malloc.\n");
        return -1;
    }
    memcpy(new_node->data, data, data_len);
    new_node->aba = 0;
    new_node->next = NULL;
    new_tail.node = new_node;
       
    do {
        do {
            tmp_tail = atomic_load(&queue->tail);
            new_tail.aba = tmp_tail.aba + 1;   
        } while(!atomic_compare_exchange_weak(&tmp_tail.node->next, &null_node, new_node));
    } while(!atomic_compare_exchange_weak(&queue->tail, &tmp_tail, new_tail));   

    LFQ_LOG_INFO("enqueue data: %d %d %d %d %d %d, aba: %"PRIuPTR"\n", 
        data[0], data[1], data[2], data[3], data[4], data[5], new_tail.aba);

    return 0;
}

int lf_queue_dequeue(queue_t *queue, char *data, size_t data_len)
{
    struct lf_queue_head new_head, origin_head;
    bool empty;

    if (queue == NULL) {
        LFQ_LOG_ERROR("queue is NULL\n");
        return -1;
    }

    if (data_len > LF_QUEUE_DATA_LEN) {
        LFQ_LOG_ERROR("datalen is %lu, max length is %d\n", data_len, LF_QUEUE_DATA_LEN);
        return -1;
    }

    empty = false;
    do {
        origin_head = atomic_load(&queue->head);
        if (origin_head.node->next == NULL) {
            /* never remove the last node */
            LFQ_LOG_INFO("empty queue\n");
            empty = true;
            break;
        }

        new_head.node = origin_head.node->next;
        new_head.aba = origin_head.aba + 1;
    } while(!atomic_compare_exchange_weak(&queue->head, &origin_head, new_head));

    if (!empty) {
        memcpy(data, origin_head.node->data, data_len);
        free(origin_head.node);
        LFQ_LOG_INFO("dequeue data: %d %d %d %d %d %d, aba: %"PRIuPTR"\n", 
            data[0], data[1], data[2], data[3], data[4], data[5], origin_head.aba);
    }

    return 0;
}

void *lf_queue_test_write_thread(void *not_used)
{
    char data[LF_QUEUE_DATA_LEN];
    int i;

    for (i = 0; i < LF_QUEUE_DATA_LEN; i++) {
        data[i] = i & 0x7f;
    }

    i = 0;
    while(1) {
        lf_queue_enqueue(&lf_queue, data, sizeof(data));

        memcpy(data, data + 4, LF_QUEUE_DATA_LEN - 4);
        data[LF_QUEUE_DATA_LEN - 4] = i++ & 0x7f;
        data[LF_QUEUE_DATA_LEN - 3] = i++ & 0x7f;
        data[LF_QUEUE_DATA_LEN - 2] = i++ & 0x7f;
        data[LF_QUEUE_DATA_LEN - 1] = i++ & 0x7f;
    }

    return NULL;
}

void *lf_queue_test_read_thread(void *not_used)
{
    char data[LF_QUEUE_DATA_LEN];

    while(1) {
        lf_queue_dequeue(&lf_queue, data, sizeof(data));
    }

    return NULL;
}

int lf_queue_test(void)
{
    pthread_t tid;

    if (lf_queue_init(&lf_queue) < 0) {
        LFQ_LOG_ERROR("fail to init lf_queue.\n");
        return -1;
    }

    pthread_create(&tid, NULL, lf_queue_test_write_thread, NULL);
    pthread_create(&tid, NULL, lf_queue_test_read_thread, NULL);

    return 0;    
}
