#include <unistd.h>

#include <pthread.h>
#include <stdbool.h>
#include <time.h>
#include <stdio.h>
#include <errno.h>



#define TIMEOUT_SECONDS 0
#define TIMEOUT_NANOSECONDS 200000000  // 200ms in nanoseconds

pthread_mutex_t subscription_mutex;
pthread_cond_t subscription_lock_cv, subscription_reading_cv;

struct {
    int reading;
    bool write_request;
    bool write_granted;
} sub_lock = {0, false, false};

void set_timer(struct timespec* ts) {
    clock_gettime(CLOCK_REALTIME, ts);
    ts->tv_sec += TIMEOUT_SECONDS;
    ts->tv_nsec += TIMEOUT_NANOSECONDS;
    if (ts->tv_nsec >= 1000000000) {
        ts->tv_nsec -= 1000000000;
        ts->tv_sec += 1;
    }
}

bool timed_wait(pthread_cond_t* cv, pthread_mutex_t* mtx, struct timespec* ts) {
    if (pthread_cond_timedwait(cv, mtx, ts) == ETIMEDOUT) {
        printf("Error: Timed out while waiting!\n");
        return false;
    }
    return true;
}

void lock_subscriptions_ro()
{
    struct timespec ts;
    set_timer(&ts);

    pthread_mutex_lock(&subscription_mutex);

    while(sub_lock.write_request) {
        if (!timed_wait(&subscription_lock_cv, &subscription_mutex, &ts)) {
            pthread_mutex_unlock(&subscription_mutex);
            return; // Or handle the timeout in some other manner
        }
    }

    sub_lock.reading++;
    pthread_mutex_unlock(&subscription_mutex);
}

void unlock_subscriptions_ro()
{
    pthread_mutex_lock(&subscription_mutex);
    sub_lock.reading--;
    if (sub_lock.reading == 0 && sub_lock.write_request) {
        pthread_cond_signal(&subscription_reading_cv);
    }
    pthread_mutex_unlock(&subscription_mutex);
}

void lock_subscriptions()
{
    struct timespec ts;
    set_timer(&ts);

    pthread_mutex_lock(&subscription_mutex);

    while(sub_lock.write_request) {
        if (!timed_wait(&subscription_lock_cv, &subscription_mutex, &ts)) {
            pthread_mutex_unlock(&subscription_mutex);
            return; // Or handle the timeout in some other manner
        }
    }

    sub_lock.write_request = true;

    while(sub_lock.reading > 0) {
        if (!timed_wait(&subscription_reading_cv, &subscription_mutex, &ts)) {
            sub_lock.write_request = false; // Reset flag if timed out
            pthread_mutex_unlock(&subscription_mutex);
            return; // Or handle the timeout in some other manner
        }
    }

    sub_lock.write_granted = true;
    pthread_mutex_unlock(&subscription_mutex);
}

void unlock_subscriptions()
{
    pthread_mutex_lock(&subscription_mutex);
    sub_lock.write_granted = false;
    sub_lock.write_request = false;
    pthread_cond_signal(&subscription_lock_cv);
    pthread_mutex_unlock(&subscription_mutex);
}

// pthread_mutex_t subscription_mutex;
// pthread_cond_t subscription_lock_cv, subscription_reading_cv;

// struct {
//     int reading;
//     bool write_request;
//     bool write_granted;
// } sub_lock = {0, false, false};  // Initialize the struct

// //read only lock of subscriptions allows multiple threads to read simultaneously
// void lock_subscriptions_ro()
// {
//     pthread_mutex_lock(&subscription_mutex);

//     while(sub_lock.write_request)
//         //conditional wait for write_request to clear
//         pthread_cond_wait(&subscription_lock_cv, &subscription_mutex);

//     sub_lock.reading++;
//     pthread_mutex_unlock(&subscription_mutex);
// }

// //free read only lock of subscriptions
// void unlock_subscriptions_ro()
// {
//     pthread_mutex_lock(&subscription_mutex);
//     sub_lock.reading--;
//     if (sub_lock.reading == 0 && sub_lock.write_request) {
//         // Signal the writer if it's waiting for all readers to finish
//         pthread_cond_signal(&subscription_reading_cv);
//     }
//     pthread_mutex_unlock(&subscription_mutex);
// }

// // lock subscriptions for editing
// void lock_subscriptions()
// {
//     pthread_mutex_lock(&subscription_mutex);

//     while(sub_lock.write_request)
//         //conditional wait for write_request to clear
//         pthread_cond_wait(&subscription_lock_cv, &subscription_mutex);

//     sub_lock.write_request = true;

//     while(sub_lock.reading > 0)
//         // conditional wait for read == 0
//         pthread_cond_wait(&subscription_reading_cv, &subscription_mutex);

//     sub_lock.write_granted = true;
//     pthread_mutex_unlock(&subscription_mutex);
// }

// // clear lock on subscriptions for editing
// void unlock_subscriptions()
// {
//     pthread_mutex_lock(&subscription_mutex);
//     sub_lock.write_granted = false;
//     sub_lock.write_request = false;
//     pthread_cond_signal(&subscription_lock_cv);  // Wake up any waiting readers or writers
//     pthread_mutex_unlock(&subscription_mutex);
// }



// // Assume these are globally defined.
// pthread_mutex_t subscription_mutex;
// pthread_cond_t subscription_lock_cv, subscription_reading_cv;
// struct {
//     int reading;
//     bool lock;
// } sub_lock;

// //read only lock of subscriptions allows multiple threads to read simultaneously
// void lock_subscriptions_ro()
// {
//     pthread_mutex_lock(&subscription_mutex);

//     while(sub_lock.lock == true)
//         //conditional wait for lock to free
//         pthread_cond_wait(&subscription_lock_cv, &subscription_mutex);

//     sub_lock.reading++;
//     pthread_mutex_unlock(&subscription_mutex);
// }

// //free read only lock of subscriptions
// void unlock_subscriptions_ro()
// {
//     pthread_mutex_lock(&subscription_mutex);
//     sub_lock.reading--;
//     pthread_cond_signal(&subscription_reading_cv);
//     pthread_mutex_unlock(&subscription_mutex);
// }

// // lock subscriptions for editing
// void lock_subscriptions()
// {
//     pthread_mutex_lock(&subscription_mutex);

//     while(sub_lock.lock == true)
//         //conditional wait for lock to free
//         pthread_cond_wait(&subscription_lock_cv, &subscription_mutex);

//     sub_lock.lock = true;

//     while(sub_lock.reading > 0)
//         // conditional wait for read == 0
//         pthread_cond_wait(&subscription_reading_cv, &subscription_mutex);

//     pthread_mutex_unlock(&subscription_mutex);
// }

// // clear lock on subscriptions for editing
// void unlock_subscriptions()
// {
//     pthread_mutex_lock(&subscription_mutex);
//     sub_lock.lock = false;
//     pthread_cond_signal(&subscription_lock_cv);
//     pthread_mutex_unlock(&subscription_mutex);
// }

void *reader_func(void *arg) {
    printf("Reader %ld trying to lock... \n", (long int)arg);
    lock_subscriptions_ro();
    printf("Reader %ld got the lock.\n", (long int)arg);

    // Simulate reading action.
    sleep(3);

    printf("Reader %ld releasing lock...\n", (long int)arg);
    unlock_subscriptions_ro();
    return NULL;
}

void *writer_func(void *arg) {
    printf("Writer trying to lock...\n");
    //sleep(2);
    lock_subscriptions();
    printf("Writer got the lock.\n");

    // Simulate write action.
    sleep(2);

    printf("Writer releasing lock...\n");
    unlock_subscriptions();
    return NULL;
}

int main() {

    printf("running\n");
    pthread_t readers[30], writer;

    pthread_mutex_init(&subscription_mutex, NULL);
    pthread_cond_init(&subscription_lock_cv, NULL);
    pthread_cond_init(&subscription_reading_cv, NULL);
    //sub_lock.reading = 0;
    //sub_lock.lock = false;
    for (int i = 0; i < 30; i++) {
        readers[i] = 0;
    }
    printf("still running\n");

    // Start two readers.
    for (int i = 0; i < 20; i++) {
        pthread_create(&readers[i], NULL, reader_func, (void*)(long int)i);
        //sleep(1); // Ensure the second reader waits until the first has locked.
    }
    printf("created readers\n");
    // Start the writer.
    pthread_create(&writer, NULL, writer_func, NULL);
    sleep(1); // Ensure the writer starts and waits for the readers to finish.

    // Start a third reader.
    pthread_create(&readers[21], NULL, reader_func, (void *)(long int)3);

    // Join all threads.
    for (int i = 0; i < 30; i++) {
        if (readers[i]) pthread_join(readers[i], NULL);
    }
    pthread_join(writer, NULL);

    return 0;
}