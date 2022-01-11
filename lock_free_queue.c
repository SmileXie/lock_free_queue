#include <string.h>
#include <pthread.h>
#include <stdatomic.h>
#include <stdint.h>
#include <stdlib.h>
#include <stdio.h>
#include <stdbool.h>
#include <inttypes.h>
#include "lock_free_queue.h"

#define LFQ_LOG_ERROR printf
#define LFQ_LOG_INFO printf

#define PLACEHOLDER_DATA 0xee

int lf_queue_init(queue_t *queue)
{
    struct lf_queue_head head_init;
    struct lf_queue_node *init_node;
    struct lf_queue_node_info info = {NULL, true, 0};

    if (queue == NULL) {
        LFQ_LOG_ERROR("queue is NULL\n");
        return -1;
    }
    
    init_node = (struct lf_queue_node *)malloc(sizeof(struct lf_queue_node));
    if (init_node == NULL) {
        LFQ_LOG_ERROR("fail to malloc.\n");
        return -1;
    }

    memset(init_node->data, PLACEHOLDER_DATA, sizeof(init_node->data));
    init_node->info = ATOMIC_VAR_INIT(info);

    head_init.aba = 0;
    head_init.node = init_node;

    queue->head = ATOMIC_VAR_INIT(head_init);
    queue->tail = ATOMIC_VAR_INIT(head_init);
    queue->size = ATOMIC_VAR_INIT(0);

    LFQ_LOG_INFO("queue %p is initialized\n", queue);

    return 0;
}

static int lf_queue_enqueue_inner(queue_t *queue, char *data, size_t data_len, bool enqueue_placeholder, bool check_head_placeholder)
{
    struct lf_queue_node *new_node, *null_node = NULL;
    struct lf_queue_head new_tail, new_head, tmp_tail, tmp_head;
    struct lf_queue_node_info new_info, tmp_info;

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

    if (!enqueue_placeholder) {
        memcpy(new_node->data, data, data_len);
        /* placeholder does not have data */
    }
    
    new_info.aba = 0;
    new_info.next = NULL;
    new_info.is_placeholder = enqueue_placeholder;
    new_node->info = ATOMIC_VAR_INIT(new_info);
       
    do {
        do {
            do {
                tmp_tail = atomic_load(&queue->tail);
                tmp_info = atomic_load(&tmp_tail.node->info);
            } while(tmp_info.next != NULL);
            
            new_info.aba = tmp_info.aba + 1;
            new_info.next = new_node;
            new_info.is_placeholder = tmp_info.is_placeholder;
        } while(!atomic_compare_exchange_weak(&tmp_tail.node->info, &tmp_info, new_info));

        new_tail.aba = tmp_tail.aba + 1;
        new_tail.node = new_node;

        /* if head is placeholder, dequeue it */
        if (!check_head_placeholder) {
            continue;
        }
        do {
            tmp_head = atomic_load(&queue->head);
            tmp_info = atomic_load(&tmp_head.node->info);
            if (!tmp_info.is_placeholder) {
                break;
            }

            LFQ_LOG_INFO("placeholder in head is found while enqueue, dequeue the head placeholder.\n");
            new_head.node = tmp_info.next;
            new_head.aba = tmp_head.aba + 1;
        } while(!atomic_compare_exchange_weak(&queue->head, &tmp_head, new_head));        
    } while(!atomic_compare_exchange_weak(&queue->tail, &tmp_tail, new_tail));   

    if (enqueue_placeholder) {
        LFQ_LOG_INFO("enqueue placeholder.\n");
    } else {
        LFQ_LOG_INFO("enqueue data: %d %d %d %d %d %d, aba: %"PRIuPTR"\n", 
            data[0], data[1], data[2], data[3], data[4], data[5], new_tail.aba);
    }
    return 0;
}

int lf_queue_enqueue(queue_t *queue, char *data, size_t data_len)
{
    return lf_queue_enqueue_inner(queue, data, data_len, false, true);
}

/* enqueue an placeholder node to push-out previous node */
static int lf_queue_enqueue_placeholder_data(queue_t *queue)
{
    char data[LF_QUEUE_DATA_LEN];
    int ret;

    memset(data, PLACEHOLDER_DATA, sizeof(data));

    ret = lf_queue_enqueue_inner(queue, data, LF_QUEUE_DATA_LEN, true, false);
    if (ret != 0) {
        LFQ_LOG_ERROR("fail to equeue empty data.\n");
        return -1;
    }

    LFQ_LOG_INFO("enqueue placeholder node.\n");

    return 0;
}

int lf_queue_dequeue(queue_t *queue, char *data, size_t data_len)
{
    struct lf_queue_head new_head, tmp_head;
    struct lf_queue_node_info tmp_info;
    int ret;

    if (queue == NULL) {
        LFQ_LOG_ERROR("queue is NULL\n");
        return -1;
    }

    if (data_len > LF_QUEUE_DATA_LEN) {
        LFQ_LOG_ERROR("datalen is %lu, max length is %d\n", data_len, LF_QUEUE_DATA_LEN);
        return -1;
    }

    do {
        tmp_head = atomic_load(&queue->head);
        tmp_info = atomic_load(&tmp_head.node->info);
       
        while (tmp_info.next == NULL) {

            if (tmp_info.is_placeholder) {
                LFQ_LOG_INFO("only node is placeholder, empty queue\n");
                return -1;
            }
            LFQ_LOG_INFO("last node, enqueue an placeholder node to push-out the node.\n");
            /* always maintain one node in queue, enqueue a placeholder to push-out the node */
            ret = lf_queue_enqueue_placeholder_data(queue);
            if (ret != 0) {
                LFQ_LOG_ERROR("fail to equeue placeholder data.\n");
                return -1;
            }
            tmp_head = atomic_load(&queue->head);
            tmp_info = atomic_load(&tmp_head.node->info);
        }

        new_head.node = tmp_info.next;
        new_head.aba = tmp_head.aba + 1;
    } while(!atomic_compare_exchange_weak(&queue->head, &tmp_head, new_head));

    memcpy(data, tmp_head.node->data, data_len);
    free(tmp_head.node);

    if (tmp_info.is_placeholder) {
        LFQ_LOG_INFO("dequeue placeholder.\n");
    } else {
        LFQ_LOG_INFO("dequeue data: %d %d %d %d %d %d, aba: %"PRIuPTR"\n", 
            data[0], data[1], data[2], data[3], data[4], data[5], tmp_head.aba);
    }

    return 0;
}
