ess_controller threads
p wilshire
08_17_2023

### Overview 

Threads are now possible. We have (almost) the DataMap concept that allows us to "do" things with blocks of data into and put of a detached namespace out of the vmap area(s).


The thing we now need to do is sync threads. 
* Setup  a thread to initialize.
* Thread waits for go from datamap
* Trigger a thread to run from waiting after vmap->name data input.
* Thread runs and wants to output data.
* Thread triggers a completion event.
* Run name->Vmap transfer and send completion event to a thread.


Threads will run a timed wait for their control channel.
The control channel will always wake up a sleeping thread with a wakeup command.
The wake up commands can be 
* transfer in ( Thread then triggers the transfer in (func) func then triggers wakeup for transfer_in done)
* transfer out ( thread triggers the transfer out (func) func then triggers wakeup for transfer_out done)
* any possible timeout monitored by the thread timeout.


* so basically we start the thread
* we give it a function to run , a transfer_in datamap , a transfer_out datamap.
* all names in the ess_thread structure
* when the thread runs it tells the scheduler to run the transfer in map and waits for the 
* wakeChan message transfer in done.
* it then runs the function
* when completed it tells the scheduer to run the transfer_out map and wait for the 
*  wakeChan message transfer out done.
* the thread then sleeps for its designated delay.
* it could be woken up , as needed , by the scheduer.


