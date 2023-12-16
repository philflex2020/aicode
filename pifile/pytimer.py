import threading
import time
import heapq
import functools

g_start_time = time.time()

class TimerRequest:
    def __init__(self, start_time, callback, interval=None, offset=None, extra_param=None):
        self.start_time = start_time
        self.callback = callback
        self.interval = interval
        self.offset = offset
        self.extra_param = extra_param
        if interval:
            xtime = time.time()
            while self.start_time + g_start_time < xtime:
                self.start_time += interval

    def __lt__(self, other):
        return self.start_time < other.start_time

def timer_thread_function():
    while True:
        current_time = time.time() - g_start_time

        with condition:
            while timer_queue and timer_queue[0].start_time <= current_time:
                timer_request = heapq.heappop(timer_queue)

                # Spawn a thread for the callback
                threading.Thread(target=timer_request.callback, 
                                 args=(timer_request, timer_request.extra_param)).start()

                # Schedule next call if it's a repeating timer
                if timer_request.interval:
                    next_time = timer_request.start_time + timer_request.interval
                    heapq.heappush(timer_queue, TimerRequest(next_time, timer_request.callback, 
                                                             timer_request.interval, timer_request.offset, 
                                                             timer_request.extra_param))

            # Wait for the next timer or until a new timer is added
            condition.wait(timeout=get_next_timeout())

def get_next_timeout():
    if timer_queue:
        next_timer = timer_queue[0]
        return max(0, next_timer.start_time - (time.time() - g_start_time))
    return None

def add_timer_request(timer_request):
    with condition:
        heapq.heappush(timer_queue, timer_request)
        condition.notify()

# Example callback function
def my_callback(timer_request, extra_param):
    print(f"Timer fired! Time: {time.time() - g_start_time}, Extra Param: {extra_param}")

# Initialize timer queue and condition
timer_queue = []
condition = threading.Condition()

# Start the timer thread
timer_thread = threading.Thread(target=timer_thread_function)
timer_thread.daemon = True
timer_thread.start()


def main():
    # Add a timer request
    add_timer_request(TimerRequest(5.0, my_callback, interval=1, extra_param=" Hello 0"))
    add_timer_request(TimerRequest(5.1, my_callback, interval=1, extra_param="    Hello 1"))
    time.sleep(10)
    add_timer_request(TimerRequest(1.3, my_callback, interval=1, extra_param="           Hello 3"))
    time.sleep(0.5)
    add_timer_request(TimerRequest(1.2, my_callback, interval=1, extra_param="        Hello 2"))
    time.sleep(20)


if __name__ == "__main__":
    main()