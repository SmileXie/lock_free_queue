#include <pthread.h>
#include <string.h>
#include <unistd.h>
#include "lock_free_queue.h"

queue_t lf_queue;

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
        return -1;
    }

    pthread_create(&tid, NULL, lf_queue_test_write_thread, NULL);
    pthread_create(&tid, NULL, lf_queue_test_read_thread, NULL);

    return 0;    
}

int main(void)
{
	lf_queue_test();
	
	while(1) {
		sleep(1000);
	}
}