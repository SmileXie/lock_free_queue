lf-queue-test: lock_free_queue.c lock_free_queue.h test.c
	gcc -o lf-queue-test lock_free_queue.c test.c -lpthread -latomic

clean:
	rm -f lf-queue-test