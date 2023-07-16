#include <iostream>
#include <chrono>
#include <functional>
#include <thread>
#include <mutex>

enum class WatchdogState {
    Fault,
    Warning,
    Recovery,
    Normal
};

class Watchdog {
public:
    Watchdog(int intervalTime, int warningTime, int faultTime, int recoveryTime,
             std::function<void(Watchdog&, void*)> warningCallback,
             std::function<void(Watchdog&, void*)> faultCallback,
             std::function<void(Watchdog&, void*)> recoveryCallback,
             std::function<void(Watchdog&, void*)> normalCallback,
             void* callbackParam)
        : intervalTime(intervalTime),
          warningTime(warningTime),
          faultTime(faultTime),
          recoveryTime(recoveryTime),
          warningCallback(warningCallback),
          faultCallback(faultCallback),
          recoveryCallback(recoveryCallback),
          normalCallback(normalCallback),
          callbackParam(callbackParam),
          currentState(WatchdogState::Normal),
          lastChangeTime(std::chrono::high_resolution_clock::now()),
          warningWait(std::chrono::high_resolution_clock::now()),
          faultWait(std::chrono::high_resolution_clock::now()),
          recoveryWait(std::chrono::high_resolution_clock::now()) {}
   

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

    void setInputValue(int value) {
        std::lock_guard<std::mutex> lock(mymutex);
        auto currentTime = std::chrono::high_resolution_clock::now();
        bool problem = true;
        if (value != inputValue) 
        {
            problem = false;
            lastChangeTime = currentTime;
            std::cout << " Value changed  : no Problem" << std::endl;
            inputValue = value;
        }    
        else
        {
            auto elapsed = std::chrono::duration_cast<std::chrono::milliseconds>(currentTime - lastChangeTime).count();
            if (elapsed < intervalTime)
            {
                std::cout << " Value not changed but within interval : no Problem" << std::endl;
                return;
            }
            std::cout << " Value not changed possible problem, Elapsed time :" << elapsed << std::endl;

        }

        if (!problem)
        {
           if (currentState == WatchdogState::Normal && warningWait<currentTime) {
                warningWait = currentTime + std::chrono::milliseconds(warningTime);
           }
           if (currentState == WatchdogState::Normal && faultWait<currentTime) {
                faultWait = currentTime + std::chrono::milliseconds(faultTime);
           }
 
           if (currentState == WatchdogState::Normal) {
                if (currentTime > warningWait) 
                {
                    currentState = WatchdogState::Warning;
                    recoveryWait = currentTime + std::chrono::milliseconds(recoveryTime);
                    if (warningCallback) 
                    {
                        warningCallback(*this, callbackParam);
                    }
                } 
                else
                {
                    std::cout << " Not reached warning : no Problem yet" << std::endl;
                }                  
            }
            if ((currentState == WatchdogState::Normal || currentState == WatchdogState::Warning) ) {
                if (currentTime > faultWait) { 
                    currentState = WatchdogState::Fault;
                    if (faultCallback) {
                        faultCallback(*this, callbackParam);
                    }
                }
                else
                {
                    std::cout << " Not reached fault : no Problem yet" << std::endl;
                }                  

            }
            if ((currentState == WatchdogState::Fault || currentState == WatchdogState::Warning) ) {
                recoveryWait = currentTime + std::chrono::milliseconds(recoveryTime);
                warningWait = currentTime + std::chrono::milliseconds(warningTime);
                faultWait = currentTime + std::chrono::milliseconds(faultTime);

                currentState = WatchdogState::Recovery;
                if (recoveryCallback) {
                    recoveryCallback(*this, callbackParam);
                }
                
                std::cout << " Started Recovery" << std::endl;
            }
            if (currentState == WatchdogState::Recovery)  {
                //recoveryWait = currentTime + std::chrono::milliseconds(recoveryTime);

                if (currentTime > recoveryWait) { 
                    currentState = WatchdogState::Normal;
                    warningWait = currentTime + std::chrono::milliseconds(warningTime);
                    faultWait = currentTime + std::chrono::milliseconds(faultTime);

                    if (normalCallback) {
                        normalCallback(*this, callbackParam);
                    }
                    std::cout << " Back to normal" << std::endl;
                }
            }

        }
        else  // we got a problem
        {
           if (currentState == WatchdogState::Normal) {
                if (currentTime > warningWait) 
                {
                    currentState = WatchdogState::Warning;
                    recoveryWait = currentTime + std::chrono::milliseconds(recoveryTime);
                    if (warningCallback)   
                    {
                        warningCallback(*this, callbackParam);
                    }
                } 
                else
                {
                    std::cout << " Not reached warning : no Problem yet" << std::endl;
                }                  
            }

            if (currentState == WatchdogState::Warning) {
                if (currentTime > faultWait) 
                {
                    currentState = WatchdogState::Fault;
                    recoveryWait = currentTime + std::chrono::milliseconds(recoveryTime);
                    if (faultCallback) 
                    {
                        faultCallback(*this, callbackParam);
                    }
                } 
                else
                {
                    std::cout << " Not reached fault : still in warning" << std::endl;
                }                  
            }
            if (currentState == WatchdogState::Recovery) {
                recoveryWait = currentTime + std::chrono::milliseconds(recoveryTime);
                std::cout << " extending recovert wait" << std::endl;
            }                  
            //}

            //     currentState = WatchdogState::Recovery;
            //     if (recoveryCallback) {
            //         recoveryCallback(*this, callbackParam);
            //     }
            //     recoveryWait = currentTime + std::chrono::milliseconds(recoveryTime);
            // }
            // if (currentState == WatchdogState::Fault) {
            //     currentState = WatchdogState::Recovery;
            //     if (recoveryCallback) {
            //         recoveryCallback(*this, callbackParam);
            //     }
            //     recoveryWait = currentTime + std::chrono::milliseconds(recoveryTime);

            // }
            // if (currentState == WatchdogState::Recovery && currentTime > recoveryWait) {
            //     currentState = WatchdogState::Normal;
            //     if (normalCallback) {
            //         normalCallback(*this, callbackParam);
            //     }
            // }
        }

        // if (value != inputValue) {
        //     lastChangeTime = currentTime;
        //     inputValue = value;

        //     if (currentState == WatchdogState::Normal && elapsed > warningTime) {
        //         currentState = WatchdogState::Warning;
        //         if (warningCallback) {
        //             warningCallback(*this, callbackParam);
        //         }
        //     }
        // }
    }

    WatchdogState getState() const {
        std::lock_guard<std::mutex> lock(mymutex);
        return currentState;
    }

private:
    int warningTime;
    int faultTime;
    int recoveryTime;
    int intervalTime;
    std::function<void(Watchdog&, void*)> warningCallback;
    std::function<void(Watchdog&, void*)> faultCallback;
    std::function<void(Watchdog&, void*)> recoveryCallback;
    std::function<void(Watchdog&, void*)> normalCallback;
    void* callbackParam;
    WatchdogState currentState;
    std::chrono::time_point<std::chrono::high_resolution_clock> lastChangeTime;
    std::chrono::time_point<std::chrono::high_resolution_clock> warningWait;
    std::chrono::time_point<std::chrono::high_resolution_clock> faultWait;
    std::chrono::time_point<std::chrono::high_resolution_clock> recoveryWait;
    int inputValue = 0;
//public:
    mutable std::mutex mymutex;
};