/**
 ******************************************************************************
 * @Copyright (C), 1997-2015, Hangzhou Gold Electronic Equipment Co., Ltd.
 * @file name: dev_can.c
 * @author: 叶建强
 * @Descriptiuon:
 *     CAN设备驱动
 * @Others: None
 * @History: 1. Created by YeJianqiang.
 * @version: V1.0.0
 * @date:    2024.1.18
 ******************************************************************************
 */
/* mofify threaded option to provide back pressure or delay to account for can send time
we have several  sources of can send depending on the can bus in use we want to return after a short delay 
to thorttle the can send rate

but 
*/
#include <string.h>
#include "dev_can.h"
#include "AppConfig.h"
#include "CanHandle.h"
#include <linux/can.h>
#include <linux/can/raw.h>


#include <pthread.h>
#include <unistd.h>
#include <sys/queue.h> // Ensure you have a compatible queue implementation

#define NUM_CANFDS 3
#define MAX_CAN_ENTRIES 512
#define CAN_RETRY_WAIT 200
#define USE_CAN_THREADS 0
// ms for max wait at 250k 500 microseconds per message
#define MAX_CAN_ENQUEUE_WAIT  1000
#define CAN_ENQUEUE_WAIT  2


static int canfd[NUM_CANFDS] = {-1,-1,-1};
//static int queue_size = 0; // Current size of the queue
volatile int can_thread_running = 1;
// 321
volatile uint16_t can_errors[NUM_CANFDS] = {0,0,0};
volatile uint16_t can_wait_l = 0;
volatile uint16_t can_wait_h = 0;
volatile uint16_t can_reads = 0;
volatile uint16_t can_writes = 0;
volatile uint16_t can_rejects = 0;
volatile uint16_t can_limits = 0;
volatile uint16_t can_queue_size[NUM_CANFDS] = {0,0,0};

volatile uint16_t can_lims[NUM_CANFDS] = {0,0,0};
static int can_drop[NUM_CANFDS] = {0,0,0};
static int can_run[NUM_CANFDS] = {0,0,0};


// Definitions for channel status
#define CAN_CHANNEL_ONLINE  1
#define CAN_CHANNEL_OFFLINE 0
// every 2 seconds
#define CAN_OFFLINE_CHECK_TIME 2    

// Global variables
int can_channel_status[NUM_CANFDS] = {
			CAN_CHANNEL_ONLINE
			, CAN_CHANNEL_ONLINE
			, CAN_CHANNEL_ONLINE
			}; // Status of each CAN channel
double next_attempt_time[NUM_CANFDS] = {0,0,0};
double next_send_time[NUM_CANFDS] = {0,0,0}; // Last send time for each channel

double last_send_time[NUM_CANFDS] = {0,0,0}; // Last send time for each channel
double can_send_delay[NUM_CANFDS] = {0.000612, 0.000612, 0.0000612}; 
double first_send_time[NUM_CANFDS] = {0,0,0}; // first send time for each channel
//double next_check_time[NUM_CANFDS] = {0,0,0}; // first send time for each channel

int can_num_send[NUM_CANFDS] = {0,0,0}; // num sends for each channel
int can_num_fails[NUM_CANFDS] = {0,0,0}; // num fails

// Mutex for thread-safe send operations
//    pthread_mutex_lock(&can_queue_mutex[index]);
pthread_mutex_t can_send_mutex[3] = {
										PTHREAD_MUTEX_INITIALIZER
										, PTHREAD_MUTEX_INITIALIZER
										, PTHREAD_MUTEX_INITIALIZER
									};


double min_can_send_delay = 0.000106; // 306 really but we need to test at 500k

//int can_check_send[NUM_CANFDS] = {0,0,0}; // num sends for each channel
//int can_check_fails[NUM_CANFDS] = {0,0,0}; // num fails


// Define a struct for the queue entry
struct can_queue_entry {
	int index;
    struct can_frame frame;
    STAILQ_ENTRY(can_queue_entry) entries; // Using BSD sys/queue.h STAILQ for simplicity
};

STAILQ_HEAD(stailq_head, can_queue_entry);

void can_sleep_ms(int ms)
{
	struct timespec ts;
	// Sleep for 100ms if no message was found to send
    // Using clock_nanosleep to be more precise and resilient against system clock changes
	clock_gettime(CLOCK_MONOTONIC, &ts);
	ts.tv_nsec += (ms * 1000000); // Add 100 ms

	if (ts.tv_nsec >= 1000000000) {
		ts.tv_sec += 1;
		ts.tv_nsec -= 1000000000;
	}
	clock_nanosleep(CLOCK_MONOTONIC, TIMER_ABSTIME, &ts, NULL);
}

// Define the head of the queue
//static STAILQ_HEAD(stailq_head, can_queue_entry) can_queue_head = STAILQ_HEAD_INITIALIZER(can_queue_head);
static struct stailq_head can_queue_head[NUM_CANFDS]; // = STAILQ_HEAD_INITIALIZER(can_queue_head);
static struct stailq_head can_free_list[3] = {
												STAILQ_HEAD_INITIALIZER(can_free_list[0])
												,STAILQ_HEAD_INITIALIZER(can_free_list[1])
												,STAILQ_HEAD_INITIALIZER(can_free_list[2])
											};

// Mutex for thread-safe queue operations
pthread_mutex_t can_queue_mutex[3] = {
										PTHREAD_MUTEX_INITIALIZER
										, PTHREAD_MUTEX_INITIALIZER
										, PTHREAD_MUTEX_INITIALIZER
									};

 //Function to initialize the CAN queue entry pool
void init_can_queue_pool(int pool_size, int index) {
    for (int i = 0; i < pool_size; i++) {
        struct can_queue_entry *entry = malloc(sizeof(struct can_queue_entry));
        if (entry) {
            STAILQ_INSERT_TAIL(&can_free_list[index], entry, entries);
        }
    }
}


// Function to get a free entry from the pool or allocate a new one
struct can_queue_entry *get_free_entry(int index) {
    pthread_mutex_lock(&can_queue_mutex[index]);
    struct can_queue_entry *entry = STAILQ_FIRST(&can_free_list[index]);
    if (entry) {
        STAILQ_REMOVE_HEAD(&can_free_list[index], entries);
    } else {
        entry = malloc(sizeof(struct can_queue_entry)); // Allocate if pool is exhausted
    }
    pthread_mutex_unlock(&can_queue_mutex[index]);
    return entry;
}

// Function to return entry to free list
void return_entry_to_free_list(struct can_queue_entry *entry, int index) {
    pthread_mutex_lock(&can_queue_mutex[index]);
    STAILQ_INSERT_TAIL(&can_free_list[index], entry, entries);
    pthread_mutex_unlock(&can_queue_mutex[index]);
}
// Function to enqueue a CAN frame
void enqueue_can_frame(int index, struct can_frame *frame) 
{
	int sleep_wait = 0;
    if (index > NUM_CANFDS) 
	{
		printf("dev_can:%s cannot enqueue; bad can frame index %d \n"
				, __func__, index);
		return;
	}
	// // skip can1 for now
    if (index > 0) 
	{
		return;
	}

    if (canfd[index] <= 0) 
	{
		printf("dev_can:%s cannot enqueue; bad canfd canfd[%d] %d\n"
				,__func__,  index, canfd[index]);
		return;
	}
    pthread_mutex_lock(&can_queue_mutex[index]);
	double send_delay = can_send_delay[index]; // = {0.000612, 0.000612, 0.0000612}; 

	if (can_queue_size[index] >= MAX_CAN_ENTRIES/2) {
		
		if(send_delay < 0.01)
		{
			can_send_delay[index] = send_delay + 0.0001;
		}
		//can_drop[index]++;
		printf("Note: Queue maxed for  can %d.  Increase delay from %f to %f mS\n"
					, index,  send_delay*1000, can_send_delay[index]*1000 );

		can_sleep_ms(1); // Sleep for 1ms to reduce pressure
		//return; // Early return if the queue is full
	}
	else if (can_queue_size[index] < 2) {
		if(send_delay > min_can_send_delay)
		{
			can_send_delay[index] = send_delay - 0.0001;
	
			printf("Note: Queue relaxed for  can %d.  Increase delay from %f to %f mS\n"
					, index,  send_delay*1000, can_send_delay[index]*1000 );
		}
	}

    
    // Check if the queue size exceeds the limit
    // todo add max delay count
	// Attempt to enqueue with backoff when queue is full
    while (can_queue_size[index] >= MAX_CAN_ENTRIES)  {
        if (!can_thread_running) {
            pthread_mutex_unlock(&can_queue_mutex[index]);
            return; // Exit without enqueuing since the thread is stopping
        }
		if ( sleep_wait >= MAX_CAN_ENQUEUE_WAIT) {
            pthread_mutex_unlock(&can_queue_mutex[index]);
			printf("dev_can:%s Timeout %d on enqueue canfd[%d] %d\n"
				,__func__,  sleep_wait, index, canfd[index]);
            return; // Exit without enqueuing since the thread is stopping
		}
		//sleep_wait+=CAN_ENQUEUE_WAIT;
        //pthread_mutex_unlock(&can_queue_mutex);
		//can_sleep_ms(CAN_ENQUEUE_WAIT); // Sleep for 20ms to reduce CPU usage while waiting
        //pthread_mutex_lock(&can_queue_mutex);
    }

    // Check if the thread is still running after waiting
    if (can_thread_running) {
        pthread_mutex_unlock(&can_queue_mutex[index]);
		struct can_queue_entry *entry = get_free_entry(index);
		pthread_mutex_lock(&can_queue_mutex[index]);

    	if (entry) {
    	     entry->index = index;
    	     memcpy(&entry->frame, frame, sizeof(struct can_frame));

	        STAILQ_INSERT_TAIL(&can_queue_head[index], entry, entries);
    	    can_queue_size[index]++;
			can_run[index] ++;

		}
	}
    pthread_mutex_unlock(&can_queue_mutex[index]);
}

    // if (can_queue_size[index] >= MAX_CAN_ENTRIES) {
	// 	can_rejects++;
    //     // Remove the oldest entry from the queue
    //     struct can_queue_entry *oldest_entry = STAILQ_FIRST(&can_queue_head[index]);
    //     if (oldest_entry) {
    //         STAILQ_REMOVE_HEAD(&can_queue_head[index], entries);
    // 		pthread_mutex_unlock(&can_queue_mutex);
    //         return_entry_to_free_list(oldest_entry);
    // 		pthread_mutex_lock(&can_queue_mutex);
    //         can_queue_size[index]--;
    //     }
    // // }

    // pthread_mutex_unlock(&can_queue_mutex);
    // struct can_queue_entry *entry = get_free_entry();
	// pthread_mutex_lock(&can_queue_mutex);

    // if (entry) {
    //     entry->index = index;
    //     memcpy(&entry->frame, frame, sizeof(struct can_frame));
    //     STAILQ_INSERT_TAIL(&can_queue_head[index], entry, entries);
    //     can_queue_size[index]++; // Increment queue size
	// 	//if (index == 0) 
	// 	if(0)printf("%s:%s enqueue can frame index %d canfd[index] %d size %d \n"
	// 						,__FILE__, __func__,  index, canfd[index], can_queue_size[index]);

    // }



void cleanup_can_queues() {
    struct can_queue_entry *entry;

	for(int i = 0 ; i < NUM_CANFDS ; i++) {
    	pthread_mutex_lock(&can_queue_mutex[i]);
    	// Free all entries in the active queue
    	while (!STAILQ_EMPTY(&can_queue_head[i])) {
        	entry = STAILQ_FIRST(&can_queue_head[i]);
        	STAILQ_REMOVE_HEAD(&can_queue_head[i], entries);
        	free(entry);
    	}

	    // Free all entries in the free list
    	while (!STAILQ_EMPTY(&can_free_list[i])) {
        	entry = STAILQ_FIRST(&can_free_list[i]);
        	STAILQ_REMOVE_HEAD(&can_free_list[i], entries);
        	free(entry);
    	}
		pthread_mutex_unlock(&can_queue_mutex[i]);

	}

}

void *can_sender_thread(void *arg) {
 	int index = *(int*)arg;  // Cast and dereference the argument to get the index
    printf("%s:%s Thread started with index: %d\n", __FILE__, __func__, index);
    struct timespec ts;
    while (can_thread_running) {
        int found = 0; 
		struct can_queue_entry *entry = NULL;
        pthread_mutex_lock(&can_queue_mutex[index]);
        if (!STAILQ_EMPTY(&can_queue_head[index])) {
            entry = STAILQ_FIRST(&can_queue_head[index]);
            STAILQ_REMOVE_HEAD(&can_queue_head[index], entries);
            found = 1;
			can_queue_size[index]--;
		}
        pthread_mutex_unlock(&can_queue_mutex[index]);
		if(found ==  1)
		{
			if (entry->index == 0) 
			{
				if(0)printf("%s:%s Try to  send CAN [%d] frame \n", __FILE__, __func__, entry->index);
			}
			else if (entry->index == 1) 
			{
				if(0)printf("%s:%s Try to  send CAN [%d] frame \n",__FILE__, __func__, entry->index);
			}
			else
			{
				printf("%s Not sending CAN [%d] frame \n", __func__, entry->index);
                return_entry_to_free_list(entry, entry->index);
				found  = 0;
			}
		}
		if(found ==  1)
		{
			int inc_sleep = 0;
            int retry_limit = 4000; // Number of milliseconds to retry
            while (retry_limit > 0) {
                int ret = write(canfd[entry->index], &entry->frame, sizeof(struct can_frame));
				int err = errno;
				if(ret < 0)
				{
					if(entry->index == 0)
					{
						// these are expected
						if(0)perror("Can 0 write fail");
					}

				}
				else
				{
					err = 0;
					if (can_send_delay[index] > min_can_send_delay)
					{
						can_send_delay[index] -= 0.00001;
					}
				}

				//if (entry->index == 0) 
				if(0)printf("%s:%s Able send CAN [%d] frame,  ret %d  err %d\n"
					, __FILE__, __func__, entry->index, ret, err);
                if (ret > 0) {
					can_writes++;
					// reset this alarm
					if(can_lims[entry->index] > 0)
					{
						can_lims[entry->index]--;
						if(can_lims[entry->index] == 0)
						    printf("%s:%s Able to send CAN [%d] frame,  resetting error check\n" 
							,__FILE__ ,__func__ ,entry->index);
					}
 
                    // Send success
                    return_entry_to_free_list(entry, entry->index);
                    break;
                } else {
                    // Send failure, retry after a small back-off
					can_wait_l++;
					if(can_wait_l==0)
						can_wait_h++;
                    can_sleep_ms(CAN_RETRY_WAIT);
                    retry_limit -= CAN_RETRY_WAIT;
					if(retry_limit < 2000 && inc_sleep == 0)
					{
						inc_sleep = 1;
						if (can_send_delay[index] < 0.001)
						{
							can_send_delay[index] += 0.00001;
						}
					}
					// if ((entry->index == 0) && (retry_limit < 2000))
					// { 						
					// 	printf("%s:%s retry send CAN [%d] frame,  limit %d \n"
					// 		, __FILE__, __func__, entry->index, retry_limit);
					// }

                }
            }

            if (retry_limit <= 0) {
                // After exhausting retries, handle the final failure case
				can_lims[entry->index]++;
				can_errors[entry->index]++;
				if(can_lims[entry->index] < 64)
				{
                	printf("%s:%s Failed to send CAN [%d] after retries, dropping frame. Dropped %d so far\n"
					,__FILE__, __func__,  entry->index, can_lims[entry->index]);
				}
                return_entry_to_free_list(entry, entry->index);
            }
        } else {
            can_sleep_ms(5); // Sleep for 5ms if no message was found
        }
    }
    printf("Can Send Thread %d Quitting\n", index);
    return NULL;
}


// fractal
uint16_t get_esmu_bcm_lan(void);

uint8_t Get_EvbcmCfg_Can0Baud(void);
uint8_t Get_EvbcmCfg_Can1Baud(void);
uint8_t Get_EvbcmCfg_Can2Baud(void);
void Set_EvbcmCfg_Can0Baud(uint8_t u8data);
void Set_EvbcmCfg_Can1Baud(uint8_t u8data);
void Set_EvbcmCfg_Can2Baud(uint8_t u8data);



// volatile uint16_t can_wait_l = 0;
// volatile uint16_t can_wait_h = 0;
// volatile uint16_t can_reads = 0;
// volatile uint16_t can_writes = 0;

static struct timespec hall_last;

int fractal_change_can = 0;    // 1 = change
int fractal_update_cancfg = 0;    // 1 = update config
pthread_mutex_t can_config_mutex = PTHREAD_MUTEX_INITIALIZER;
volatile int can_send_ok = 1;

int Can_Rx_Hook(int can, Can_Frame_Def* p_can_frame)
{
	int ret;
	uint8_t data[64];
	struct timespec now;
	int cost;

#if 0
	DEBUG("[%d] CAN frame: [can%d][%08x] %d bytes:\n", mcore_started, can, p_can_frame->id, p_can_frame->len);
	Hexdump(p_can_frame->data, p_can_frame->len);
#endif

	/* Hall CAN frame very fast, limit the speed */
	if (p_can_frame->id == 0x3c2)
	{
		clock_gettime(CLOCK_MONOTONIC, &now);
		cost = getcost(hall_last, now);
		if (cost<30)
		{
//			DEBUG_CYAN("Hall interval %dms\n", cost);
			return 0;
		}
		hall_last = now;
	}

	strcpy(data, "can");
	data[3] = p_can_frame->len;
	data[4] = can;
	data[5] = (p_can_frame->id>>24)&0xff;
	data[6] = (p_can_frame->id>>16)&0xff;
	data[7] = (p_can_frame->id>>8)&0xff;
	data[8] = p_can_frame->id&0xff;
	data[9] = p_can_frame->frametype;
	if (p_can_frame->len) memcpy(data+10, p_can_frame->data, p_can_frame->len);
//	Hexdump(data, 10+p_can_frame->len);

	return mcore_send(data, 10+p_can_frame->len);
}

static int check_canup(int port)
{
	int sock;
	int ret;
	struct ifreq ifr;

	sock = socket(PF_CAN, SOCK_RAW, CAN_RAW);
	if (sock<0) return sock;

	sprintf(ifr.ifr_name, "can%d", port);
	ret = ioctl(sock, SIOCGIFFLAGS, &ifr);
//	DEBUG("ifr->ifr_flags %x (%d)\n", ifr.ifr_flags, ifr.ifr_flags&IFF_UP);
	if(sock > 0)close(sock);
	return ret<0 ? 0 : (ifr.ifr_flags&IFF_UP);
}

static int create_canfd(int port)
{
	int sock;
	int ret;
	struct sockaddr_can addr;
	struct ifreq ifr;
	int loopback = 0;

	sock = socket(PF_CAN, SOCK_RAW, CAN_RAW);
	if (sock<0) 
	{
		printf( " socket failed for can %d\n",port);
		return sock;
	}
	sprintf(ifr.ifr_name, "can%d", port);
	ret = ioctl(sock, SIOCGIFINDEX, &ifr);
	if (ret<0)
	{
		printf( " ifindex ioctl failed for can %d\n",port);
		return ret;
	}
	addr.can_family = AF_CAN;
	addr.can_ifindex = ifr.ifr_ifindex;

	ret = ioctl(sock, SIOCGIFFLAGS, &ifr);
	if (ret<0)
	{
		printf( " get flags ioctl failed for can %d\n",port);
		return ret;
	}

	
	ret = bind(sock, (struct sockaddr*)&addr, sizeof(addr));
	if (ret<0)
	{
		printf( " get bind failed for can %d\n",port);
		return ret;
	}

	ret = setsockopt(sock, SOL_CAN_RAW, CAN_RAW_LOOPBACK, &loopback, sizeof(loopback));
	if (ret<0)
	{
		printf( " raw loopback failed for can %d\n",port);
		return ret;
	}
	printf( " %s passed for can %d fd %d\n",__func__, port, sock);
	return sock;
}

static int can_rxhandler(void* arg)
{
	int ret;
	struct can_frame frame;
	Can_Frame_Def can_frame;
	int index = (int)(uint64_t)arg;

	
	ret = read(canfd[index], &frame, sizeof(frame));
	if (ret<0)
	{
		ERROR("read error (%m)\n");
		return ret;
	}
	can_reads++;
	if(0)printf(" %s:%s received can message %d from can%d mcore_started %d frame_id %x \n"
					,__FILE__, __func__, can_reads, index, mcore_started, frame.can_id);
	/* ignore if mcore not initialized */
	if (!mcore_started) return 0;

	/* 0x3c2 for Hall */
	can_frame.id 	= frame.can_id & (~(1<<31));
	can_frame.len 	= frame.can_dlc;
	can_frame.frametype = can_frame.id==0x3c2 ? CAN_STD_DATA_FRAME : CAN_EXT_DATA_FRAME;
	memcpy(can_frame.data, frame.data, frame.can_dlc);
	
#if 0
	if (can_frame.id != 0x3c2)
	{
		WARNING("CAN frame: [can%d][%08x] %d bytes:\n", index, can_frame.id, frame.can_dlc);
		Hexdump(frame.data, frame.can_dlc);
	}
#endif

	/* CAN1 for ESMU/PC/PCS, CAN0 for Hall/BMM */
	// BSW/BasicSoft/src/CanHandle.c:
#if 1
	Process_Can_Rx_Frame(index, &can_frame);
#else
	Can_Rx_Hook(index, &can_frame);
#endif
	return 0;
}


// Delay in seconds for each channel @ 250k

double can_time_dbl() {
    struct timespec ts;
    clock_gettime(CLOCK_MONOTONIC, &ts);
    return (double)ts.tv_sec + (double)ts.tv_nsec / 1e9;
}


static int check_canup(int port);
static struct timespec can_last;
//static struct timespec hall_last;
//af_can
/* send would fail if no peer exist, remember to connect ESMU or PC */
static int send_can_frame(int index, struct can_frame* frame)
{
	if (index > NUM_CANFDS)
	{
		return 0;
	}

	double current_time = can_time_dbl();
    pthread_mutex_lock(&can_send_mutex[index]);
	if((can_channel_status[index] == CAN_CHANNEL_OFFLINE) && (current_time < next_attempt_time[index])) 
	{
		pthread_mutex_unlock(&can_send_mutex[index]);
		return 0;
	}

	// force this to happen
	can_channel_status[index] = CAN_CHANNEL_ONLINE;
	next_attempt_time[index] = current_time + 5.0;

	// we can send but we need to wait can_send_delay
	if (next_send_time[index] >  current_time )
	{
        usleep(can_send_delay[index] - ((next_send_time[index] - current_time) * 1e6)); // Delay in microseconds
	}

	next_send_time[index] = current_time + can_send_delay[index];
	
	if(first_send_time[index] == 0)
	{
		first_send_time[index] = current_time;
	}

	double time_since_first_send = current_time - first_send_time[index];

	// if(USE_CAN_THREADS)
	// {
	// 	enqueue_can_frame(index, frame); 
	// 	return 0;
	// }
    // code replaced
	int ret;
	int retry = 100;
	static int flag = 0; /* prohibit many error messsages */
	if (index>1 || canfd[index]<=0 || !frame) 
	{
		pthread_mutex_unlock(&can_send_mutex[index]);
		return -EINVAL;
	}
// TODO fractal just allow can 0 for now
	//if (index>0 ) return -EINVAL;
	struct timespec per_now;
	clock_gettime(CLOCK_MONOTONIC, &per_now);
	int per_cost = getcost(can_last, per_now);
	clock_gettime(CLOCK_MONOTONIC, &can_last);

	while (can_send_ok)
	{
		if((can_num_send[index]%1000) == 0)
		{
			printf("CAN  [can%d] up_time %f; used retry %d delay uS %f num_send %d num_fail %d:\r\n"
					, index,  time_since_first_send, retry, can_send_delay[index] * 1e6
					, can_num_send[index], can_num_fails[index] );
		}

		if(0) printf("%s:%s trying to send can data index %d fd %d\n"
			, __FILE__,__func__, index, canfd[index]);
		ret = write(canfd[index], frame, sizeof(struct can_frame));
		if(0) printf("%s:%s after send  can data index %d fd %d ret %d\n"
						,__FILE__,__func__, index, canfd[index], ret);
		if (ret>0)
		{
			if (can_send_delay[index] > min_can_send_delay)
			{
				can_send_delay[index] -= 0.000001;
			}
			if(retry < 100)
			{
				DEBUG("CAN  [can%d] up_time %f; used retry %d delay uS %f num_send %d num_fail %d:\r\n"
						, index,  time_since_first_send, retry, can_send_delay[index] * 1e6, can_num_send[index], can_num_fails[index] );
			}
			can_num_send[index]++; 
			can_writes++;
			flag = 0;
			break;
		}

		/* wait awhile for CAN bus idle */
		--retry;
		if (retry && can_send_ok)
		{
			can_wait_l++;
			if(can_wait_l==0)
				can_wait_h++;

			usleep(500);

			if(retry <= 99)
			{
				DEBUG("CAN  [can%d] up_time %f; retry %d delay uS %f num_send %d num_fail %d:\r\n"
					, index,  time_since_first_send, retry, can_send_delay[index] * 1e6, can_num_send[index], can_num_fails[index] );
				if (can_send_delay[index] < 0.001)
				{
					can_send_delay[index] += 0.00001;
				}
			}
			continue;
		}
        // now is the interface up ??
		// ip -details link show can0
		// look for error active
		//  can state ERROR-ACTIVE (berr-counter tx 0 rx 0) restart-ms 100
		//
		can_num_fails[index]++; 
		if (flag < 500)
		{
			
			struct timespec now;
			clock_gettime(CLOCK_MONOTONIC, &now);
			int cost = getcost(can_last, now);
			flag ++;
			ERROR("send CAN frame failed (%m), please make sure ESMU/PC/BCM exists\n");
			int canup = check_canup(index);
			DEBUG("flag %d CAN frame: [can%d][%08x] %d bytes; canup %d cost %d per %d:\r\n"
					, flag, index, frame->can_id, frame->can_dlc, canup, cost, per_cost);
			Hexdump(frame->data, frame->can_dlc);
			DEBUG("CAN  [can%d] up_time %f; num_send %d num_fail %d:\r\n"
					, index,  time_since_first_send, can_num_send[index], can_num_fails[index] );
		}
		can_channel_status[index] = CAN_CHANNEL_OFFLINE;
    	pthread_mutex_unlock(&can_send_mutex[index]);

		return -EIO;
	}
    pthread_mutex_unlock(&can_send_mutex[index]);
	return 0;
}

int mcore_can_handler(uint8_t* data, int len)
{
	int ret;
	int index;
	struct can_frame frame;
	uint32_t id_header = 0;
	if (!data) return -EINVAL;


	if (len<10)
	{
		//WARNING("len %d\n", len);
		//Hexdump(data, len);
		return 0;
	}

	/* only CAN0/CAN1 */
	index = data[4]?1:0;

	if(0) printf("%s:%s  index %d \n",__FILE__, __func__, index);

	frame.can_id = 1<<31 | data[5]<<24 | data[6]<<16 | data[7]<<8 | data[8];
	frame.can_dlc = data[3];
	if (len<10+frame.can_dlc)
	{
//		WARNING("len %d\n", len);
		return 0;
	}

	/* send ack back */
	mcore_send("cak", 3);

// fractal
//#ifdef CONFIG_ESMU_BCM_LAN
	if (get_esmu_bcm_lan())
	{
	/* ignore alarm/cutoff CAN frames */
		id_header = frame.can_id & 0xffffff00;
		if (data[4]==2 && (id_header==0x9838f400 || id_header==0x983ff400)) return 10+frame.can_dlc;
	}
//#endif

	if (frame.can_dlc) memcpy(frame.data, data+10, frame.can_dlc);
#if 1
	static int frames = 0;
	if (frames < 10)
	{
		frames++;
		if (index && (data[6]==0x37||data[6]==0x3a))
		{
			WARNING(">>>>>>>>>>>> frames %d [can%d][%08x]\n", frames, data[4], frame.can_id);
			Hexdump(frame.data, frame.can_dlc);
		}
	}
#endif

	send_can_frame(index, &frame);
	return 10+frame.can_dlc;
}


pthread_t send_thread_id[NUM_CANFDS];
int send_indicies[NUM_CANFDS];


void init_can_sender() {
	for (int i = 0; i < NUM_CANFDS; i++) {
		init_can_queue_pool(32, i);
        STAILQ_INIT(&can_queue_head[i]); // Initialize each queue head
    }
	for (int i = 0 ; i < NUM_CANFDS ; i++)
	{
		send_indicies[i] = i;
    	pthread_create(&send_thread_id[i], NULL, can_sender_thread, (void*)&send_indicies[i]);
    	pthread_detach(send_thread_id[i]); // Detach the thread since we won't be joining it
	}
}

int init_can(void)
{
	int rc;
	pthread_mutex_lock(&can_config_mutex);
	fractal_change_can  = 0;
	pthread_mutex_unlock(&can_config_mutex);
	canfd[0] = -1;
	canfd[1] = -1;
	canfd[2] = -1;

	uint8_t can_speed_0 = Get_EvbcmCfg_Can0Baud();
	uint8_t can_speed_1 = Get_EvbcmCfg_Can1Baud();
	uint8_t can_speed_2 = Get_EvbcmCfg_Can2Baud();

	if(0)printf(" %s can 0 [%d] can 1 [%d] can 2 [%d]\n"
			, __func__ 
			, can_speed_0
			, can_speed_1
			, can_speed_2
			);
	/* can must be up, or TCP socket works not well */
	//if (!check_canup(0))
	if(1)
	{

		if(0)printf(" setting up can 0 \n");
		rc = system("ifconfig can0 down");
		switch (can_speed_0) {
			case 1:
				rc = system("ip link set can0 type can bitrate 125000");
				break;
			case 2:
				rc = system("ip link set can0 type can bitrate 250000");
				break;
			case 4:
				rc = system("ip link set can0 type can bitrate 500000");
				break;
			case 8:
				rc = system("ip link set can0 type can bitrate 1000000");
				break;
			default:
				rc = system("ip link set can0 type can bitrate 250000");
				Set_EvbcmCfg_Can0Baud(2);

				break;
		}
// #ifdef CONFIG_CAN_500K
// 		rc = system("ip link set can0 type can bitrate 500000");
// #else
// 		rc = system("ip link set can0 type can bitrate 250000"); /* hall must be 250K */
// #endif
		rc = system("ip link set can0 type can restart-ms 100");
		rc = system("ifconfig can0 up");
	}
	else
	{
		printf(" NOT setting up can 0 \n");

	}
 
	//if (rc == 0)
	{
		canfd[0] = create_canfd(0);
	}

	//if (!check_canup(1))
	if(1)
	{
		if(0)printf(" setting up can 1 \n");
		rc = system("ifconfig can1 down");
		switch (can_speed_1) {
			case 1:
				rc = system("ip link set can1 type can bitrate 125000");
				break;
			case 2:
				rc = system("ip link set can1 type can bitrate 250000");
				break;
			case 4:
				rc = system("ip link set can1 type can bitrate 500000");
				break;
			case 8:
				rc = system("ip link set can1 type can bitrate 1000000");
				break;
			default:
				rc = system("ip link set can1 type can bitrate 250000");
				Set_EvbcmCfg_Can1Baud(2);
				break;
		}

		rc = system("ip link set can1 type can restart-ms 100");
		rc = system("ifconfig can1 up");
	}
	else
	{
		printf(" NOT setting up can 1 \n");

	}

	//if (rc == 0)
	{
		canfd[1] = create_canfd(1);
	}

	
	//if ((can_speed_2 > 0) && !check_canup(2))
	if ((can_speed_2 > 0)) // && !check_canup(2))
	{

		if(0)printf(" setting up can 2 \n");

		rc = system("ifconfig can2 down");
		switch (can_speed_0) {
			case 1:
				rc = system("ip link set can2 type can bitrate 125000");
				break;
			case 2:
				rc = system("ip link set can2 type can bitrate 250000");
				break;
			case 4:
				rc = system("ip link set can2 type can bitrate 500000");
				break;
			case 8:
				rc = system("ip link set can2 type can bitrate 1000000");
				break;
			default:
				rc = system("ip link set can2 type can bitrate 250000");
				Set_EvbcmCfg_Can2Baud(2);
				break;
		}
// #ifdef CONFIG_CAN_500K
// 		rc = system("ip link set can0 type can bitrate 500000");
// #else
// 		rc = system("ip link set can0 type can bitrate 250000"); /* hall must be 250K */
// #endif
		rc = system("ip link set can2 type can restart-ms 100");
		rc = system("ifconfig can2 up");
	}
 	else
	{
		printf(" NOT setting up can 2 \n");
	}

	//if (rc == 0)
	{
		canfd[2] = create_canfd(2);
	}


	if (canfd[0]<0 || canfd[1]<0)
	{
		can_send_ok = 0;
		ERROR("create_canfd failed\n");
		return -EIO;
	}

	init_can_sender();
// fractal can3
//	DEBUG("%d/%d\n", canfd[0], canfd[1]);
	printf(" can fds [%d] [%d] [%d] \n", canfd[0], canfd[1], canfd[2]);

	if(canfd[0]> 0)
		add_loopfd(canfd[0], POLLIN, &can_rxhandler, (void *)0);
	if(canfd[1]> 0)
		add_loopfd(canfd[1], POLLIN, &can_rxhandler, (void*)1);

	if ((can_speed_2 > 0) && canfd[2]>0)
	{
		add_loopfd(canfd[2], POLLIN, &can_rxhandler, (void*)2);
	}

	clock_gettime(CLOCK_MONOTONIC, &hall_last);
	can_send_ok = 1;
	return 0;
}

void uninit_can(void)
{
	can_send_ok = 0;
	can_thread_running = 0;
	can_sleep_ms(150);

	if (canfd[0]>0)
	{
		close(canfd[0]);
		remove_loopfd(canfd[0]);
		canfd[0] = -1;
	}
	if (canfd[1]>0)
	{
		close(canfd[1]);
		remove_loopfd(canfd[1]);
		canfd[1] = -1;
	}
	if (canfd[2]>0)
	{
		close(canfd[2]);
		remove_loopfd(canfd[2]);
		canfd[2] = -1;
	}
}

/* *****************************************************
功能: 向CAN2 写入数据
返回值:1--成功,0--失败(溢出)
****************************************************** */



//Can_Chl_Def
//int Send_Can_Tx_Frame(int can, Can_Frame_Def* p_message)
int Send_Can_Tx_Frame(Can_Chl_Def can, Can_Frame_Def* p_message)
{
	struct can_frame frame;
	int ret;
	int retry = 10;
	static int flag = 0;
	if (can>CAN_CHL_CAN2 || !p_message) 
		return -EINVAL;

	/* redirect CAN2 to CAN1 */
	if (can == CAN_CHL_CAN2) can = CAN_CHL_CAN1;


	frame.can_id 	= 1<<31 | p_message->id;
	frame.can_dlc 	= p_message->len;
	memcpy(frame.data, p_message->data, frame.can_dlc);
#if 0
	WARNING("[%08x]\n", frame.can_id);
	Hexdump(frame.data, frame.can_dlc);
#endif
	if(0) printf("%s:%s  can%d \n",__FILE__, __func__, can);

	send_can_frame(can, &frame);
	return 0;
}


void Modify_Can0_Baud(Can_Baud_Def can_baud)
{
}

void Modify_Can1_Baud(Can_Baud_Def can_baud)
{
}

void Modify_Can2_Baud(Can_Baud_Def can_baud)
{
}

