write a c++  function to manage a vector or timer objects. 
Use a high_resolution_clock 
Use c++17 and dont forget to include thread.
Each timer object will have an id string, a start time, an expire time, a reload time and a callback function with a void* callback parameter. The objects are sorted in increasing expire time order. The management thread  will wake up and either process the callback function or service a management object that will add, delete or modify timer objects on the list . If a timer object has  a reload time  of zero the management thread will delete the timer from the vector. If   the timer object has a start time the management thread will delay processing the timer object until the start time.
Adding a new timer object or modifying an existing timer object will cause the management thread to be woken up to service any alterations to the list. 
Add a command to wake up the management thread and delete all items in the last and terminate the thread.
Add a command to show all the items in the timer list
Add a command to delete all the items in the timer list

Write a main program to create a socket on port 2022 to service  commands like show , add , modify to access the management thread.
 ask the socket  interface  for all fields for the add , modify and delete commands

write gtest code to test the function.
Use a command line option to select test or socket mode.
When in socket mode show a prompt with the socket port on the command line
Create a Makefile and show me how to run gtest