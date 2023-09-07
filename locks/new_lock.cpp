#include <stdio.h>
#include <stdlib.h>
#include <pthread.h>

typedef struct {
    pthread_mutex_t mutex;
    pthread_cond_t readers_cv;
    pthread_cond_t writer_cv;
    int readers;
    bool writer;
    int writers_waiting;
} rwlock_t;

void init_rwlock(rwlock_t *lock) {
    pthread_mutex_init(&lock->mutex, NULL);
    pthread_cond_init(&lock->readers_cv, NULL);
    pthread_cond_init(&lock->writer_cv, NULL);
    lock->readers = 0;
    lock->writer = false;
    lock->writers_waiting = 0;
}

void lock_read(rwlock_t *lock) {
    pthread_mutex_lock(&lock->mutex);
    while (lock->writer || lock->writers_waiting > 0)
        pthread_cond_wait(&lock->readers_cv, &lock->mutex);
    lock->readers++;
    pthread_mutex_unlock(&lock->mutex);
}

void unlock_read(rwlock_t *lock) {
    pthread_mutex_lock(&lock->mutex);
    lock->readers--;
    if (lock->readers == 0)
        pthread_cond_signal(&lock->writer_cv);
    pthread_mutex_unlock(&lock->mutex);
}

void lock_write(rwlock_t *lock) {
    pthread_mutex_lock(&lock->mutex);
    lock->writers_waiting++;
    while (lock->writer || lock->readers > 0)
        pthread_cond_wait(&lock->writer_cv, &lock->mutex);
    lock->writers_waiting--;
    lock->writer = true;
    pthread_mutex_unlock(&lock->mutex);
}

void unlock_write(rwlock_t *lock) {
    pthread_mutex_lock(&lock->mutex);
    lock->writer = false;
    pthread_cond_broadcast(&lock->readers_cv);
    pthread_cond_signal(&lock->writer_cv);
    pthread_mutex_unlock(&lock->mutex);
}
// Example data structure and its associated lock.
typedef struct {
    int *data;
    int count;
    int capacity;
    rwlock_t lock;
} IntList;

// Initializes the list.
void init_list(IntList *list, int capacity) {
    list->data = (int *) malloc(sizeof(int) * capacity);
    list->count = 0;
    list->capacity = capacity;
    init_rwlock(&list->lock);
}

// Adds an integer to the list (writer operation).
void add_to_list(IntList *list, int value) {
    lock_write(&list->lock);
    if (list->count < list->capacity) {
        list->data[list->count++] = value;
    }
    // In a real-world scenario, you'd want to handle the case where
    // the list is full, maybe by reallocating memory.
    unlock_write(&list->lock);
}

// Reads and prints the list (reader operation).
void print_list(IntList *list) {
    lock_read(&list->lock);
    for (int i = 0; i < list->count; i++) {
        printf("%d ", list->data[i]);
    }
   //if(list->count > 0)
   printf("\n");
    unlock_read(&list->lock);
}

// Sample reader thread function.
void *reader_thread(void *arg) {
    IntList *list = (IntList *) arg;
    for (int i = 0; i < 5; i++) {
        print_list(list);
    }
    return NULL;
}

// Sample writer thread function.
void *writer_thread(void *arg) {
    IntList *list = (IntList *) arg;
    for (int i = 0; i < 5; i++) {
        add_to_list(list, i);
    }
    return NULL;
}

int main() {
    IntList list;
    init_list(&list, 100);

    pthread_t readers[10], writers[5];

    // Creating multiple reader and writer threads.
    for (int i = 0; i < 10; i++) {
        pthread_create(&readers[i], NULL, reader_thread, &list);
    }
    for (int i = 0; i < 5; i++) {
        pthread_create(&writers[i], NULL, writer_thread, &list);
    }

    // Waiting for all threads to finish.
    for (int i = 0; i < 10; i++) {
        pthread_join(readers[i], NULL);
    }
    for (int i = 0; i < 5; i++) {
        pthread_join(writers[i], NULL);
    }

    // Clean up.
    free(list.data);

    return 0;
}