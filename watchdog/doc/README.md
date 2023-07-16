User
write a c++  class  to manage a watchdog object.
 Use a high_resolution_clock 
Use c++17 and dont forget to include thread.
The Watchdog object  has four states Fault, Warning, Recovery and Normal
The watchdog object gets an integer value which is expected to change within a designated time period.
If no change is detected in the warning time the watchdog enters the warning state and executes a warning callback with the watchdog object and a void* as parameters.
If no change is detected in the alarm time the watchdog enters the alarm state and executes a fault  callback with the watchdog object and a void* as parameters.
If the watchdog object is in a fault or warning state and detects an input change it enters the recovery state for the recovery time period and executes a recovery callback with the watchdog object and a void * as parameters.
If the input continues to change during the recovery time  the watchdog will enter an normal state and executes a normal callback with the watchdog object and a void * as parameters.
Document the code 

// value not changed after interval time == problem
   // if problem
   // if (normal)
   // if warning wait == 0
   //   set warningwait to now + warningTime 
   // if faultwait == 0
   //   set faultwait to now + faultTime 
   // if now > warningwait
   //  set state to warning  run warning callback
   //   set recoverywait to now + recovery time 
   // if now > faultwait
   //  set state to fault  run fault callback
   //   set recoverywait to now + recovery time 
   // 
   // if (warning or fault or recovery)
   //   set recoverywait to now + recovery time 


   // if (!problem)
   // if (alarm or warning )
   //  run recovery callback
   //   set state recovery
   // if (recovery)
   //    if now > recoverywait
   //      set state normal
  //       run normal callback
  //       set recoverywait to 0
   //   
