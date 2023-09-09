#include <pthread.h>
#include <stdio.h>
#include <unistd.h>
#include <ostream>

int debug = 0;
// Assume these are globally defined.
pthread_mutex_t subscription_mutex;
pthread_cond_t subscription_lock_cv, subscription_reading_cv;
struct {
    int reading;
    bool lock;
    bool write_request;
    bool write_granted;
} sub_lock;


// old code
//read only lock of subscriptions allows multiple threads to read simultaneously
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

// //read only lock of subscriptions allows multiple threads to read simultaneously
// void lock_subscriptions_ro()
// {
//     pthread_mutex_lock(&subscription_mutex);

//     while(sub_lock.write_request == true)
//         //conditional wait for lock to free
//         pthread_cond_wait(&subscription_lock_cv, &subscription_mutex);

//     sub_lock.reading++;
//     pthread_mutex_unlock(&subscription_mutex);
// }
void lock_subscriptions_ro()
{
    pthread_mutex_lock(&subscription_mutex);

    while(sub_lock.write_request)
        //conditional wait for write_request to clear
        pthread_cond_wait(&subscription_lock_cv, &subscription_mutex);

    sub_lock.reading++;
    pthread_mutex_unlock(&subscription_mutex);
}

//free read only lock of subscriptions
void unlock_subscriptions_ro()
{
    pthread_mutex_lock(&subscription_mutex);
    sub_lock.reading--;
    if (sub_lock.reading == 0 && sub_lock.write_request) {
        // Signal the writer if it's waiting for all readers to finish
        pthread_cond_signal(&subscription_reading_cv);
    }
    pthread_mutex_unlock(&subscription_mutex);
}

// lock subscriptions for editing
void lock_subscriptions()
{
    pthread_mutex_lock(&subscription_mutex);

    while(sub_lock.write_request)
        //conditional wait for write_request to clear
        pthread_cond_wait(&subscription_lock_cv, &subscription_mutex);

    sub_lock.write_request = true;

    while(sub_lock.reading > 0)
        // conditional wait for read == 0
        pthread_cond_wait(&subscription_reading_cv, &subscription_mutex);

    sub_lock.write_granted = true;
    pthread_mutex_unlock(&subscription_mutex);
}

// clear lock on subscriptions for editing
void unlock_subscriptions()
{
    pthread_mutex_lock(&subscription_mutex);
    sub_lock.write_granted = false;
    sub_lock.write_request = false;
    pthread_cond_signal(&subscription_lock_cv);  // Wake up any waiting readers or writers
    pthread_mutex_unlock(&subscription_mutex);
}


void *reader_func(void *arg) {
    if(debug)printf("Reader %ld trying to lock... \n", (long int)arg);
    lock_subscriptions_ro();
    if(debug)printf("Reader %ld got the lock.\n", (long int)arg);

    // Simulate reading action.
    sleep(3);

    if(debug)printf("Reader %ld releasing lock...\n", (long int)arg);
    unlock_subscriptions_ro();
    if(debug)printf("Reader %ld released lock...\n", (long int)arg);
    return NULL;
}

void *writer_func(void *arg) {
    if(debug)printf("Writer %ld trying to lock...\n", (long int)arg);
    //sleep(2);
    lock_subscriptions();
    if(debug)printf("Writer %ld got the lock.\n", (long int)arg);

    // Simulate write action.
    sleep(2);

    if(debug)printf("Writer %ld  releasing lock...\n", (long int)arg);
    unlock_subscriptions();
    if(debug)printf("Writer %ld released lock...\n"), (long int)arg;
    return NULL;
}

pthread_t readers[30], writers[30];

void run_locks()
    {
    for (int i = 0; i < 30; i++) {
        readers[i] = 0;
        writers[i] = 0;
    }

    // Start two readers.
    for (int i = 0; i < 20; i++) {
        pthread_create(&readers[i], NULL, reader_func, (void*)(long int)i);
        //sleep(1); // Ensure the second reader waits until the first has locked.
    }

    // Start the writer.
    for (int i = 0; i < 1; i++) {
       pthread_create(&writers[i], NULL, writer_func, (void *)(long int)i);
    //sleep(1); // Ensure the writer starts and waits for the readers to finish.
    }
    // Start a third reader.
    pthread_create(&readers[21], NULL, reader_func, (void *)(long int)21);
    pthread_create(&readers[22], NULL, reader_func, (void *)(long int)22);

    // Join all threads.
    for (int i = 0; i < 30; i++) {
        if(readers[i])
            pthread_join(readers[i], NULL);
        if(writers[i])
            pthread_join(writers[i], NULL);
    }
//    pthread_join(writer, NULL);
    printf(" all done\n");
    return;
    }

int main(int argc, char * argv[]) {
    if (argc > 1) debug = 1;
    pthread_mutex_init(&subscription_mutex, NULL);
    pthread_cond_init(&subscription_lock_cv, NULL);
    pthread_cond_init(&subscription_reading_cv, NULL);
    sub_lock.reading = 0;
    sub_lock.lock = false;
    sub_lock.write_request =  false;
    sub_lock.write_granted = false;
    for (int i = 0 ; i < 100; i++)
        run_locks();
return 0;
}