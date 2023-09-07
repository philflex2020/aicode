Your code is attempting to implement a readers-writer lock, where multiple readers can acquire the lock at the same time, but a writer must wait for all readers to finish before it can acquire the lock, and readers must wait for the writer to finish if it has the lock.

```
//read only lock of subscriptions allows multiple threads to read simultaneously
void lock_subscriptions_ro()
{
    pthread_mutex_lock(&subscription_mutex);

    while(sub_lock.lock == true)
        //conditional wait for lock to free
        pthread_cond_wait(&subscription_lock_cv, &subscription_mutex);

    sub_lock.reading++;
    pthread_mutex_unlock(&subscription_mutex);
}

//free read only lock of subscriptions
void unlock_subscriptions_ro()
{
    pthread_mutex_lock(&subscription_mutex);
    sub_lock.reading--;
    pthread_cond_signal(&subscription_reading_cv);
    pthread_mutex_unlock(&subscription_mutex);
}

// lock subscriptions for editing
void lock_subscriptions()
{
    pthread_mutex_lock(&subscription_mutex);

    while(sub_lock.lock == true)
        //conditional wait for lock to free
        pthread_cond_wait(&subscription_lock_cv, &subscription_mutex);

    sub_lock.lock = true;

    while(sub_lock.reading > 0)
        // conditional wait for read == 0
        pthread_cond_wait(&subscription_reading_cv, &subscription_mutex);

    pthread_mutex_unlock(&subscription_mutex);
}

// clear lock on subscriptions for editing
void unlock_subscriptions()
{
    pthread_mutex_lock(&subscription_mutex);
    sub_lock.lock = false;
    pthread_cond_signal(&subscription_lock_cv);
    pthread_mutex_unlock(&subscription_mutex);
}
```

Upon inspection, I can see several issues with the code:

1. **Lack of Definitions and Structure**: It is assumed that `sub_lock`, `subscription_mutex`, `subscription_lock_cv`, and `subscription_reading_cv` are already defined somewhere, probably as global variables. It would be clearer and safer to encapsulate them in a structure, and pass a pointer to that structure to each function. This would also make it easier to have multiple such locks if needed.

2. **Missing Broadcaster Signal in `unlock_subscriptions_ro`**: In `unlock_subscriptions_ro`, when a reader releases the lock, and potentially when there are no more readers, you should use `pthread_cond_broadcast` instead of `pthread_cond_signal` to wake up all waiting writers. This is because if you have multiple writers waiting and only use `pthread_cond_signal`, only one will be woken up, but then it might still have to wait if there are other readers.

3. **Potential Deadlock**: Consider this scenario:
    - One or more readers have the read lock.
    - A writer comes in, sets `sub_lock.lock` to `true` and waits for all readers to finish.
    - Another reader comes in. It sees `sub_lock.lock` as `true` and waits.
    - Now, the writer is waiting for all readers to finish, but new readers are also waiting because of the writer lock flag. This is a deadlock.

   To avoid this, you need a mechanism that prevents new readers from acquiring the read lock when there's a writer waiting. One way to achieve this is by introducing another variable (e.g., `writers_waiting`) to keep track of waiting writers. Readers should then only acquire the lock if `sub_lock.lock` is false and `writers_waiting` is zero.

4. **Possible Race Condition**: You signal `subscription_reading_cv` in `unlock_subscriptions_ro` when a reader unlocks. However, the writer only waits on `subscription_reading_cv` after setting `sub_lock.lock = true;`. If a reader unlocks between these two operations, the writer might miss the signal and keep waiting even when there are no readers.

5. **Misleading Comments**: Your comments mention "editing" the subscriptions, but the code doesn't provide any direct operations to edit the subscriptions. These comments might confuse other developers who work on this code.

6. **Condition Variable Usage**: The general guideline for using condition variables is to associate them with a specific condition on shared state. This condition should be checked in a loop, as you're doing. However, the condition should be a function of shared state, not the condition variable itself. Ensure your conditional checks are based on the state, not on past signals.

Here's a revised approach addressing these issues:

```c
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
```

This solution introduces a `writers_waiting` counter to ensure that when a writer is waiting, readers wait for their turn. It also uses `pthread_cond_broadcast` to signal all waiting threads when the writer unlocks, to ensure that both waiting writers and readers get the signal.


Certainly! I'll provide an example of how to use the revised reader-writer locks `rwlock_t` from the earlier answer.

Imagine you have a simple shared data structure, a list of integers. Multiple threads can read the list, but only one can modify it at any given time. 

```c
#include <stdio.h>
#include <stdlib.h>
#include <pthread.h>

// The reader-writer lock definition (as provided in the previous answer).
// ... (Place the `rwlock_t` definition and associated functions here) ...

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

    pthread_t readers[5], writers[2];

    // Creating multiple reader and writer threads.
    for (int i = 0; i < 5; i++) {
        pthread_create(&readers[i], NULL, reader_thread, &list);
    }
    for (int i = 0; i < 2; i++) {
        pthread_create(&writers[i], NULL, writer_thread, &list);
    }

    // Waiting for all threads to finish.
    for (int i = 0; i < 5; i++) {
        pthread_join(readers[i], NULL);
    }
    for (int i = 0; i < 2; i++) {
        pthread_join(writers[i], NULL);
    }

    // Clean up.
    free(list.data);

    return 0;
}
```

In this example, multiple reader threads are reading the list simultaneously while writer threads are trying to modify the list. The reader-writer locks (`rwlock_t`) ensure that:

- Multiple readers can read simultaneously without any writer interference.
- When a writer is modifying the list, no reader or other writer can access the list.

This ensures the integrity of the data while allowing concurrent access, optimizing for the often more frequent read operations.

Certainly! I'll provide an example of how to use the revised reader-writer locks `rwlock_t` from the earlier answer.

Imagine you have a simple shared data structure, a list of integers. Multiple threads can read the list, but only one can modify it at any given time. 

```c
#include <stdio.h>
#include <stdlib.h>
#include <pthread.h>

// The reader-writer lock definition (as provided in the previous answer).
// ... (Place the `rwlock_t` definition and associated functions here) ...

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

    pthread_t readers[5], writers[2];

    // Creating multiple reader and writer threads.
    for (int i = 0; i < 5; i++) {
        pthread_create(&readers[i], NULL, reader_thread, &list);
    }
    for (int i = 0; i < 2; i++) {
        pthread_create(&writers[i], NULL, writer_thread, &list);
    }

    // Waiting for all threads to finish.
    for (int i = 0; i < 5; i++) {
        pthread_join(readers[i], NULL);
    }
    for (int i = 0; i < 2; i++) {
        pthread_join(writers[i], NULL);
    }

    // Clean up.
    free(list.data);

    return 0;
}
```

In this example, multiple reader threads are reading the list simultaneously while writer threads are trying to modify the list. The reader-writer locks (`rwlock_t`) ensure that:

- Multiple readers can read simultaneously without any writer interference.
- When a writer is modifying the list, no reader or other writer can access the list.

This ensures the integrity of the data while allowing concurrent access, optimizing for the often more frequent read operations.

Of course! Here's a Google Test (`gtest`) setup to test the `IntList` structure and the reader-writer locks (`rwlock_t`):

1. First, let's set up the test environment.

You should have Google Test installed. If you don't, you can usually install it using a package manager or from source as detailed in its GitHub repository.

2. Organize your code.

For clarity, separate your implementation from the testing code. For this example:

- `rwlock.h` and `rwlock.c` for the `rwlock_t` definition and associated functions.
- `intlist.h` and `intlist.c` for the `IntList` definition and its operations.

3. Create a test file: `intlist_test.cpp`.

```cpp
#include <gtest/gtest.h>
#include "intlist.h"

// Test initialization of IntList.
TEST(IntListTest, Initialization) {
    IntList list;
    init_list(&list, 10);
    EXPECT_EQ(list.count, 0);
    EXPECT_EQ(list.capacity, 10);
    free(list.data);
}

// Test adding elements to IntList.
TEST(IntListTest, AddToList) {
    IntList list;
    init_list(&list, 10);

    add_to_list(&list, 5);
    EXPECT_EQ(list.count, 1);
    EXPECT_EQ(list.data[0], 5);

    add_to_list(&list, 6);
    EXPECT_EQ(list.count, 2);
    EXPECT_EQ(list.data[1], 6);

    free(list.data);
}

// Here you can add more tests for other operations or edge cases.

int main(int argc, char **argv) {
    ::testing::InitGoogleTest(&argc, argv);
    return RUN_ALL_TESTS();
}
```

4. Compile and Run:

To compile, you'd link against the Google Test libraries. A typical compilation might look like:

```bash
g++ -std=c++11 -o intlist_test intlist_test.cpp intlist.c rwlock.c -lgtest -lgtest_main -lpthread
```

Then run the test:

```bash
./intlist_test
```

This will give you an output indicating which tests passed and which failed.

A few points to remember:

- Always clean up any dynamically allocated memory in your tests to prevent memory leaks.
- Google Test provides a wide array of assertions (`EXPECT_EQ`, `EXPECT_TRUE`, etc.) to help make your tests expressive and comprehensive.
- You might want to extend your tests to ensure that the reader-writer locks work correctly in multi-threaded scenarios, though such tests can be a bit more complex due to the inherent challenges of concurrent programming.

If you encounter any issues related to linking or using `gtest`, ensure that you've set up and installed Google Test properly in your development environment.


Certainly! To demonstrate the issue with the original code, you'd want to create a scenario where:

1. Multiple readers have acquired read locks.
2. A writer attempts to get the write lock and starts waiting because there are active readers.
3. A new reader tries to acquire a read lock but also gets stuck waiting because the writer has set `sub_lock.lock` to `true`.

The third step is the problematic one; in a correct implementation, this new reader should wait until after the writer has obtained and released its lock. But in your original code, the writer is waiting for all readers to release their locks, while the new reader is also waiting because the writer's lock is set. This will cause a deadlock.

Here's the code to simulate this situation:

```c
#include <pthread.h>
#include <stdio.h>
#include <unistd.h>

// Assume these are globally defined.
pthread_mutex_t subscription_mutex;
pthread_cond_t subscription_lock_cv, subscription_reading_cv;
struct {
    int reading;
    bool lock;
} sub_lock;

//read only lock of subscriptions allows multiple threads to read simultaneously
void lock_subscriptions_ro()
{
    pthread_mutex_lock(&subscription_mutex);

    while(sub_lock.lock == true)
        //conditional wait for lock to free
        pthread_cond_wait(&subscription_lock_cv, &subscription_mutex);

    sub_lock.reading++;
    pthread_mutex_unlock(&subscription_mutex);
}

//free read only lock of subscriptions
void unlock_subscriptions_ro()
{
    pthread_mutex_lock(&subscription_mutex);
    sub_lock.reading--;
    pthread_cond_signal(&subscription_reading_cv);
    pthread_mutex_unlock(&subscription_mutex);
}

// lock subscriptions for editing
void lock_subscriptions()
{
    pthread_mutex_lock(&subscription_mutex);

    while(sub_lock.lock == true)
        //conditional wait for lock to free
        pthread_cond_wait(&subscription_lock_cv, &subscription_mutex);

    sub_lock.lock = true;

    while(sub_lock.reading > 0)
        // conditional wait for read == 0
        pthread_cond_wait(&subscription_reading_cv, &subscription_mutex);

    pthread_mutex_unlock(&subscription_mutex);
}

// clear lock on subscriptions for editing
void unlock_subscriptions()
{
    pthread_mutex_lock(&subscription_mutex);
    sub_lock.lock = false;
    pthread_cond_signal(&subscription_lock_cv);
    pthread_mutex_unlock(&subscription_mutex);
}
// ... (Place your original lock and unlock functions here) ...

void *reader_func(void *arg) {
    printf("Reader trying to lock... %d\n", (int)arg);
    lock_subscriptions_ro();
    printf("Reader got the lock.\n");

    // Simulate reading action.
    sleep(2);

    printf("Reader releasing lock...%d\n", (int)arg);
    unlock_subscriptions_ro();
    return NULL;
}

void *writer_func(void *arg) {
    printf("Writer trying to lock...\n");
    lock_subscriptions();
    printf("Writer got the lock.\n");

    // Simulate write action.
    sleep(2);

    printf("Writer releasing lock...\n");
    unlock_subscriptions();
    return NULL;
}

int main() {
    pthread_t readers[3], writer;

    pthread_mutex_init(&subscription_mutex, NULL);
    pthread_cond_init(&subscription_lock_cv, NULL);
    pthread_cond_init(&subscription_reading_cv, NULL);
    sub_lock.reading = 0;
    sub_lock.lock = false;

    // Start two readers.
    for (int i = 0; i < 2; i++) {
        pthread_create(&readers[i], NULL, reader_func, 1);
        sleep(1); // Ensure the second reader waits until the first has locked.
    }

    // Start the writer.
    pthread_create(&writer, NULL, writer_func, NULL);
    sleep(1); // Ensure the writer starts and waits for the readers to finish.

    // Start a third reader.
    pthread_create(&readers[2], NULL, reader_func, 2);

    // Join all threads.
    for (int i = 0; i < 3; i++) {
        pthread_join(readers[i], NULL);
    }
    pthread_join(writer, NULL);

    return 0;
}
```

In this setup, the first two readers will get the read lock, and then the writer will attempt to get the write lock but will have to wait. Finally, the third reader will try to get the read lock but will also wait because the writer has set the `sub_lock.lock` to `true`. This situation will lead to a deadlock where the writer is waiting for all readers (including the third one) to finish, but the third reader is waiting for the writer to finish.

You can further observe the issue by noting that after the "Writer trying to lock..." message, you'll see the "Reader trying to lock..." message from the third reader, but neither the writer nor the third reader will ever print that they have acquired the lock, indicating the deadlock.


Great! Your code now has a higher likelihood of experiencing a deadlock because of the following conditions:

1. Multiple readers (20 in total) are trying to obtain the read lock almost simultaneously due to the removed sleep between the reader thread creations.
2. The writer thread will get scheduled among these readers and will attempt to get the write lock.
3. With this setup, there's a chance that one or more readers grab the read lock after the writer has begun waiting for the write lock but before the remaining readers have finished.

The deadlock scenario unfolds as:

1. A couple of readers acquire the read lock and increment the reading count.
2. The writer then tries to acquire the write lock but sees that readers have the read lock (because `sub_lock.reading` is greater than 0), so it waits.
3. Another reader tries to get the read lock. It sees the `sub_lock.lock` is still false (because the writer has not actually acquired the lock yet, it's just waiting for it), so it attempts to acquire the read lock. 
4. This continues for other readers as well, preventing the reading count from reaching 0, which in turn prevents the writer from ever acquiring the write lock.
5. As a result, we end up with a deadlock where the writer is waiting for the reading count to be 0, but it never becomes 0 because other readers keep getting the read lock after seeing `sub_lock.lock` is still false.

This is a classic writer starvation scenario where readers keep coming in and holding the lock, causing the writer to wait indefinitely.

To mitigate such scenarios in real-world systems, you could employ various strategies, like:

1. **Writer Priority**: This involves giving priority to writers over readers. When a writer wants to write, incoming readers would wait, allowing the writer to complete its task.
2. **Fairness**: Implement a fair reader-writer lock where the order of acquiring locks is determined by the order of requests (like a FIFO queue).
3. **Limiting the number of concurrent readers**: A fixed number of readers can acquire the read lock at any time. If the limit is reached, subsequent readers would have to wait.

The exact strategy to use would depend on the specific requirements and characteristics of the system you're working on.
Using two flags for the write lock, i.e., "write request" and "write granted" can indeed help to simplify the logic and reduce the contention between readers and writers in certain scenarios. The idea you're suggesting is similar to the "readers-writers problem" with writer priority, where once a writer indicates its intention to write, new readers are blocked until the writer is done. This is particularly helpful to prevent "write starvation," which can occur when continuous read locks prevent a writer from getting its turn.

Your suggested approach can be described as follows:

1. When a writer wants to write:
    - It first sets the "write request" flag. This blocks new readers from acquiring the lock.
    - If there are ongoing reads (i.e., read count > 0), the writer then waits. Once the last reader releases the lock, the "write granted" flag is set, and the writer can proceed.
    - After writing, the writer clears both the "write request" and "write granted" flags.

2. When a reader wants to read:
    - It first checks the "write request" flag. If it's set, the reader waits.
    - Otherwise, it increments the read count and proceeds.
    - After reading, it decrements the read count. If the read count goes to zero and the "write request" flag is set, it sets the "write granted" flag to allow the waiting writer to proceed.

To answer your second question: yes, all flags and counts should be atomic, and operations on them should be thread-safe. This ensures that no two threads see an inconsistent state and that updates to these shared variables are immediately visible to all threads. You can achieve this using atomic operations, mutexes, or other synchronization primitives depending on the specifics of your programming environment.

In C++, you can make use of the `<atomic>` header, which provides `std::atomic` and related functions, to make certain operations atomic.

However, remember that merely making variables atomic does not solve all concurrency problems. You still need to ensure that the overall logic of acquiring and releasing locks, as well as the ordering of operations, is correct. Properly synchronizing multi-threaded code can be complex, and it's essential to think through all possible interleavings of operations to ensure correctness.

Let's refactor the code based on the two-flag mechanism that you suggested: "write request" and "write granted".

Here's how we will modify the code:

1. Add two flags: `write_request` and `write_granted`.
2. The `lock_subscriptions` function will set `write_request`. New readers will be blocked once `write_request` is set. If there are ongoing reads, this function will wait until all readers are done. Once all readers are done, `write_granted` is set.
3. The `unlock_subscriptions` function will clear both `write_request` and `write_granted`.
4. The `lock_subscriptions_ro` function will check `write_request`. If it's set, the reader will wait. Otherwise, it will increment the reading count.
5. The `unlock_subscriptions_ro` function will decrement the reading count. If the reading count becomes zero and `write_request` is set, it will signal to the writer that it can proceed.

Here's the modified code:

```c
#include <pthread.h>
#include <stdbool.h>

pthread_mutex_t subscription_mutex;
pthread_cond_t subscription_lock_cv, subscription_reading_cv;

struct {
    int reading;
    bool write_request;
    bool write_granted;
} sub_lock = {0, false, false};  // Initialize the struct

//read only lock of subscriptions allows multiple threads to read simultaneously
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
```

This code now ensures that:

- Once a writer indicates a wish to write, new readers will be blocked.
- If there are ongoing reads, the writer will wait until all of them finish.
- After writing, the writer will release both flags.
- Readers will check the `write_request` flag. If it's set, they'll wait. Otherwise, they'll proceed. After reading, they'll signal the writer if needed.

Alright, we can achieve this by recording timestamps before and after attempting to acquire a lock and then calculating the elapsed time. For storing and computing the statistics, we can use a struct that maintains a sum of all durations, max, min, and count.

Here's a modification of your code:

```c
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

struct {
    double max_duration;
    double min_duration;
    double total_duration;
    int count;
} lock_stats = {0, __DBL_MAX__, 0, 0};

void record_duration(double duration) {
    lock_stats.count++;
    lock_stats.total_duration += duration;
    if (duration > lock_stats.max_duration) lock_stats.max_duration = duration;
    if (duration < lock_stats.min_duration) lock_stats.min_duration = duration;
}

void display_lock_stats() {
    printf("Lock Statistics:\n");
    printf("Max Duration: %f\n", lock_stats.max_duration);
    printf("Min Duration: %f\n", lock_stats.min_duration);
    printf("Average Duration: %f\n", lock_stats.total_duration / lock_stats.count);
    printf("Total Acquires: %d\n", lock_stats.count);
}

double get_duration(struct timespec start, struct timespec end) {
    return (end.tv_sec - start.tv_sec) + (end.tv_nsec - start.tv_nsec) / 1e9;
}

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
    struct timespec start, end;
    clock_gettime(CLOCK_REALTIME, &start);

    int result = pthread_cond_timedwait(cv, mtx, ts);

    clock_gettime(CLOCK_REALTIME, &end);
    record_duration(get_duration(start, end));

    if (result == ETIMEDOUT) {
        printf("Error: Timed out while waiting!\n");
        return false;
    }
    return true;
}

// ... Rest of your locking functions remain the same ...

// Somewhere in your main or where appropriate
// display_lock_stats();
```

This code measures the time spent waiting for each lock, and updates the max, min, and average durations. You can display these statistics using the `display_lock_stats()` function.


Certainly! By creating a `Perf` class, we can encapsulate the timing operations. By starting the timer in the constructor and stopping it in the destructor, we ensure the time spent in the block where the `Perf` object is instantiated is measured. Then, the destructor will update the statistics.

Here's how you can implement this:

```c++
#include <iostream>
#include <chrono>
#include <string>
#include <limits>

class Perf {
    std::string label;
    std::chrono::high_resolution_clock::time_point start;

    struct Stats {
        double max_duration = 0;
        double min_duration = std::numeric_limits<double>::max();
        double total_duration = 0;
        int count = 0;
    };
    static Stats stats;

public:
    Perf(const std::string& label_) : label(label_) {
        start = std::chrono::high_resolution_clock::now();
    }

    ~Perf() {
        auto end = std::chrono::high_resolution_clock::now();
        double duration = std::chrono::duration<double>(end - start).count();
        record_duration(duration);
        std::cout << "Time for " << label << ": " << duration << " seconds." << std::endl;
    }

    void record_duration(double duration) {
        stats.count++;
        stats.total_duration += duration;
        if (duration > stats.max_duration) stats.max_duration = duration;
        if (duration < stats.min_duration) stats.min_duration = duration;
    }

    static void display_statistics() {
        std::cout << "Performance Statistics:\n";
        std::cout << "Max Duration: " << stats.max_duration << " seconds\n";
        std::cout << "Min Duration: " << stats.min_duration << " seconds\n";
        std::cout << "Average Duration: " << stats.total_duration / stats.count << " seconds\n";
        std::cout << "Total Measurements: " << stats.count << std::endl;
    }
};

Perf::Stats Perf::stats;  // Define static member

// Usage:

int main() {
    {
        Perf myperf("Block 1");
        // other stuff
    }

    {
        Perf myperf("Block 2");
        // other stuff
    }

    Perf::display_statistics();

    return 0;
}
```

Whenever you instantiate a `Perf` object in a block, the timer will start, and once the block is exited (and the `Perf` object goes out of scope), the destructor will compute the duration and display it.

You can then call `Perf::display_statistics()` to see the min, max, and average timings of all `Perf` instances.
Certainly! We can enhance the `Perf` class to maintain a map of labels to their corresponding statistics. This way, each labeled block will have its own set of statistics. Here's an updated version of the class:

```c++
#include <iostream>
#include <chrono>
#include <string>
#include <limits>
#include <map>

class Perf {
    std::string label;
    std::chrono::high_resolution_clock::time_point start;

    struct Stats {
        double max_duration = 0;
        double min_duration = std::numeric_limits<double>::max();
        double total_duration = 0;
        int count = 0;

        void record_duration(double duration) {
            count++;
            total_duration += duration;
            if (duration > max_duration) max_duration = duration;
            if (duration < min_duration) min_duration = duration;
        }
    };

    static std::map<std::string, Stats> labeled_stats;

public:
    Perf(const std::string& label_) : label(label_) {
        start = std::chrono::high_resolution_clock::now();
    }

    ~Perf() {
        auto end = std::chrono::high_resolution_clock::now();
        double duration = std::chrono::duration<double>(end - start).count();
        labeled_stats[label].record_duration(duration);
        std::cout << "Time for " << label << ": " << duration << " seconds." << std::endl;
    }

    static void display_statistics() {
        std::cout << "Performance Statistics:\n";
        for (const auto& pair : labeled_stats) {
            const std::string& label = pair.first;
            const Stats& stats = pair.second;
            std::cout << "Label: " << label << std::endl;
            std::cout << "\tMax Duration: " << stats.max_duration << " seconds\n";
            std::cout << "\tMin Duration: " << stats.min_duration << " seconds\n";
            std::cout << "\tAverage Duration: " << stats.total_duration / stats.count << " seconds\n";
            std::cout << "\tTotal Measurements: " << stats.count << std::endl;
        }
    }
};

std::map<std::string, Perf::Stats> Perf::labeled_stats;  // Define static member

// Usage:

int main() {
    {
        Perf myperf("Block 1");
        // other stuff
    }

    {
        Perf myperf("Block 2");
        // other stuff
    }

    Perf::display_statistics();

    return 0;
}
```

Now, the `Perf` class uses a map where each label (string) is mapped to its corresponding statistics (`Stats`). When the destructor of the `Perf` object is called, the statistics of the respective label are updated. The `display_statistics()` function then provides an overview of the statistics for each label.

Certainly! I will modify the timed lock example to use the newly designed `Perf` class for performance measurements. Here's the updated code:

```c++
#include <iostream>
#include <pthread.h>
#include <chrono>
#include <string>
#include <map>
#include <limits>

#define TIMEOUT_SECONDS 0
#define TIMEOUT_NANOSECONDS 200000000  // 200ms in nanoseconds

pthread_mutex_t subscription_mutex;
pthread_cond_t subscription_lock_cv, subscription_reading_cv;

struct {
    int reading;
    bool write_request;
    bool write_granted;
} sub_lock = {0, false, false};

class Perf {
    std::string label;
    std::chrono::high_resolution_clock::time_point start;

    struct Stats {
        double max_duration = 0;
        double min_duration = std::numeric_limits<double>::max();
        double total_duration = 0;
        int count = 0;

        void record_duration(double duration) {
            count++;
            total_duration += duration;
            if (duration > max_duration) max_duration = duration;
            if (duration < min_duration) min_duration = duration;
        }
    };

    static std::map<std::string, Stats> labeled_stats;

public:
    Perf(const std::string& label_) : label(label_) {
        start = std::chrono::high_resolution_clock::now();
    }

    ~Perf() {
        auto end = std::chrono::high_resolution_clock::now();
        double duration = std::chrono::duration<double>(end - start).count();
        labeled_stats[label].record_duration(duration);
    }

    static void display_statistics() {
        std::cout << "Performance Statistics:\n";
        for (const auto& pair : labeled_stats) {
            const std::string& label = pair.first;
            const Stats& stats = pair.second;
            std::cout << "Label: " << label << std::endl;
            std::cout << "\tMax Duration: " << stats.max_duration << " seconds\n";
            std::cout << "\tMin Duration: " << stats.min_duration << " seconds\n";
            std::cout << "\tAverage Duration: " << stats.total_duration / stats.count << " seconds\n";
            std::cout << "\tTotal Measurements: " << stats.count << std::endl;
        }
    }
};

std::map<std::string, Perf::Stats> Perf::labeled_stats;  // Define static member

void lock_subscriptions_ro() {
    Perf perf("lock_subscriptions_ro");
    pthread_mutex_lock(&subscription_mutex);
    while (sub_lock.write_request) {
        pthread_cond_wait(&subscription_lock_cv, &subscription_mutex);
    }
    sub_lock.reading++;
    pthread_mutex_unlock(&subscription_mutex);
}

void unlock_subscriptions_ro() {
    Perf perf("unlock_subscriptions_ro");
    pthread_mutex_lock(&subscription_mutex);
    sub_lock.reading--;
    if (sub_lock.reading == 0 && sub_lock.write_granted) {
        pthread_cond_signal(&subscription_lock_cv);
    }
    pthread_mutex_unlock(&subscription_mutex);
}

void lock_subscriptions() {
    Perf perf("lock_subscriptions");
    pthread_mutex_lock(&subscription_mutex);
    while (sub_lock.write_request) {
        pthread_cond_wait(&subscription_lock_cv, &subscription_mutex);
    }
    sub_lock.write_request = true;
    while (sub_lock.reading > 0) {
        pthread_cond_wait(&subscription_reading_cv, &subscription_mutex);
    }
    sub_lock.write_granted = true;
    pthread_mutex_unlock(&subscription_mutex);
}

void unlock_subscriptions() {
    Perf perf("unlock_subscriptions");
    pthread_mutex_lock(&subscription_mutex);
    sub_lock.write_request = false;
    sub_lock.write_granted = false;
    pthread_cond_signal(&subscription_lock_cv);
    pthread_mutex_unlock(&subscription_mutex);
}

int main() {
    // Testing code or usage example

    Perf::display_statistics();

    return 0;
}
```

Now, every time any of the lock functions are called, a `Perf` object is created and the time spent in that function is recorded. At the end, you can call `Perf::display_statistics()` to see the performance measurements for each locking function.

Alright, I'll structure the solution in the following manner:

1. `Perf` class will generate a message when its destructor is called.
2. We'll use a separate thread to monitor messages from all `Perf` instances.
3. This thread will update the global map with the performance metrics.

To manage the inter-thread communication, we'll employ a thread-safe queue (let's name it `RecordQueue`). This queue will contain messages about the label and its recorded time duration. The record thread will constantly poll this queue and update the stats.

Let's implement this:

```c++
#include <iostream>
#include <pthread.h>
#include <chrono>
#include <string>
#include <map>
#include <limits>
#include <queue>
#include <mutex>
#include <condition_variable>

class RecordQueue {
    struct Record {
        std::string label;
        double duration;
    };

    std::queue<Record> records;
    std::mutex mtx;
    std::condition_variable cv;

public:
    void push(const std::string& label, double duration) {
        {
            std::lock_guard<std::mutex> lock(mtx);
            records.push({label, duration});
        }
        cv.notify_one();
    }

    bool pop(Record& record) {
        std::unique_lock<std::mutex> lock(mtx);
        cv.wait(lock, [this]() { return !records.empty(); });

        record = records.front();
        records.pop();

        return true;
    }
};

RecordQueue recordQueue;

class Perf {
    std::string label;
    std::chrono::high_resolution_clock::time_point start;

public:
    Perf(const std::string& label_) : label(label_) {
        start = std::chrono::high_resolution_clock::now();
    }

    ~Perf() {
        auto end = std::chrono::high_resolution_clock::now();
        double duration = std::chrono::duration<double>(end - start).count();
        recordQueue.push(label, duration);
    }
};

struct Stats {
    double max_duration = 0;
    double min_duration = std::numeric_limits<double>::max();
    double total_duration = 0;
    int count = 0;

    void record_duration(double duration) {
        count++;
        total_duration += duration;
        if (duration > max_duration) max_duration = duration;
        if (duration < min_duration) min_duration = duration;
    }
};

std::map<std::string, Stats> labeled_stats;

void* record_thread_func(void* arg) {
    RecordQueue::Record record;
    while (recordQueue.pop(record)) {
        labeled_stats[record.label].record_duration(record.duration);
    }
    return nullptr;
}

void display_statistics() {
    std::cout << "Performance Statistics:\n";
    for (const auto& pair : labeled_stats) {
        const std::string& label = pair.first;
        const Stats& stats = pair.second;
        std::cout << "Label: " << label << std::endl;
        std::cout << "\tMax Duration: " << stats.max_duration << " seconds\n";
        std::cout << "\tMin Duration: " << stats.min_duration << " seconds\n";
        std::cout << "\tAverage Duration: " << stats.total_duration / stats.count << " seconds\n";
        std::cout << "\tTotal Measurements: " << stats.count << std::endl;
    }
}

int main() {
    pthread_t recordThread;
    pthread_create(&recordThread, nullptr, record_thread_func, nullptr);

    // Testing code or usage example

    pthread_join(recordThread, nullptr);

    display_statistics();

    return 0;
}
```

This implementation has the advantage of offloading the recording to another thread. Still, ensure that the record thread has time to process all messages in the queue. Depending on how frequently you generate messages and how long the recording process is, you may need to introduce more advanced techniques (e.g., batching) to manage the workload.
Batching is a technique where you accumulate a set of tasks or messages and then process or dispatch them in one go. This can be a performance optimization in certain scenarios as it reduces the number of times you need to acquire locks or wake up a thread.

To implement batching for the `Perf` class results:

1. Instead of pushing individual `Record` items to the `RecordQueue`, we can push batches (vectors) of `Record` items.
2. The `record_thread_func` will then process all items in a batch at once.

Here's the modified version with batching:

```c++
#include <iostream>
#include <pthread.h>
#include <chrono>
#include <string>
#include <map>
#include <limits>
#include <queue>
#include <mutex>
#include <condition_variable>
#include <vector>

const size_t BATCH_SIZE = 100;  // Change according to your needs

class RecordQueue {
    struct Record {
        std::string label;
        double duration;
    };

    std::vector<Record> batch;
    std::queue<std::vector<Record>> batches;
    std::mutex mtx;
    std::condition_variable cv;

public:
    void push(const std::string& label, double duration) {
        std::lock_guard<std::mutex> lock(mtx);

        batch.push_back({label, duration});

        if (batch.size() >= BATCH_SIZE) {
            batches.push(std::move(batch));
            batch.clear();
            cv.notify_one();
        }
    }

    bool pop(std::vector<Record>& records_batch) {
        std::unique_lock<std::mutex> lock(mtx);
        cv.wait(lock, [this]() { return !batches.empty(); });

        records_batch = std::move(batches.front());
        batches.pop();

        return true;
    }

    // Force push remaining records, for graceful termination
    void flush() {
        std::lock_guard<std::mutex> lock(mtx);
        if (!batch.empty()) {
            batches.push(std::move(batch));
            cv.notify_one();
        }
    }
};

RecordQueue recordQueue;

class Perf {
    std::string label;
    std::chrono::high_resolution_clock::time_point start;

public:
    Perf(const std::string& label_) : label(label_) {
        start = std::chrono::high_resolution_clock::now();
    }

    ~Perf() {
        auto end = std::chrono::high_resolution_clock::now();
        double duration = std::chrono::duration<double>(end - start).count();
        recordQueue.push(label, duration);
    }
};

struct Stats {
    double max_duration = 0;
    double min_duration = std::numeric_limits<double>::max();
    double total_duration = 0;
    int count = 0;

    void record_duration(double duration) {
        count++;
        total_duration += duration;
        if (duration > max_duration) max_duration = duration;
        if (duration < min_duration) min_duration = duration;
    }
};

std::map<std::string, Stats> labeled_stats;

void* record_thread_func(void* arg) {
    std::vector<RecordQueue::Record> records_batch;
    while (recordQueue.pop(records_batch)) {
        for (const auto& record : records_batch) {
            labeled_stats[record.label].record_duration(record.duration);
        }
    }
    return nullptr;
}

void display_statistics() {
    std::cout << "Performance Statistics:\n";
    for (const auto& pair : labeled_stats) {
        const std::string& label = pair.first;
        const Stats& stats = pair.second;
        std::cout << "Label: " << label << std::endl;
        std::cout << "\tMax Duration: " << stats.max_duration << " seconds\n";
        std::cout << "\tMin Duration: " << stats.min_duration << " seconds\n";
        std::cout << "\tAverage Duration: " << stats.total_duration / stats.count << " seconds\n";
        std::cout << "\tTotal Measurements: " << stats.count << std::endl;
    }
}

int main() {
    pthread_t recordThread;
    pthread_create(&recordThread, nullptr, record_thread_func, nullptr);

    // Testing code or usage example
    // ... your code ...

    recordQueue.flush();
    pthread_join(recordThread, nullptr);

    display_statistics();

    return 0;
}
```

With this implementation, the `Perf` class accumulates records until it reaches `BATCH_SIZE` and then pushes the batch to the queue. The separate thread processes each batch at once, reducing the number of times it has to wake up. The `flush()` function ensures that any remaining records are pushed to the queue when the main function ends.