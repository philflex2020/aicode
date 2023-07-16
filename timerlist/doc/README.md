To compile and run the GTest code, perform the following steps:

Save the code in a file called timer_manager_test.cpp.

Save the Makefile in the same directory as timer_manager_test.cpp.

Open a terminal and navigate to the directory containing the files.

Run the following command to compile the code and generate the executable:

shell
Copy code
make
Once the compilation is successful, execute the following command to run the GTest:

shell
Copy code
./timer_manager_test
You should see the test results displayed in the terminal.

To run the program in socket mode, use the following command:

shell
Copy code
./timer_manager_test -s <port_number>
Replace <port_number> with the desired port number for the socket server.

For example, to run the program in socket mode on port 2022, use:

shell
Copy code
./timer_manager_test -s 2022
The program will display a prompt with the socket port on the command line. You can then use a tool like Telnet or Netcat to connect to the socket server and send commands like "show", "add", "modify", etc.