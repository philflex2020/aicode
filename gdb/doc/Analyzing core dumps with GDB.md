Analyzing core dumps with GDB can be a powerful way to diagnose issues in your program. To inspect the state of various threads and examine structure elements, follow these steps:

1. **Loading the Core Dump**
   
   Start GDB with both your executable and the core dump as arguments:
   ```bash
   gdb /path/to/your/executable /path/to/core/dump
   ```

2. **Listing the Threads**

   Once you've loaded your core dump, you can see a list of all threads with:
   ```bash
   (gdb) info threads
   ```

3. **Switching to a Specific Thread**

   To switch to a specific thread, use the `thread` command followed by the thread number:
   ```bash
   (gdb) thread [thread-number]
   ```

   For example, to switch to thread 3:
   ```bash
   (gdb) thread 3
   ```

4. **Examining Stack Frames**

   Once in a thread, you can see its stack frames with:
   ```bash
   (gdb) bt
   ```

   This shows a backtrace of all function calls leading up to the current location for that thread.

5. **Examining Variables and Structures**

   To examine a specific variable or structure, use the `print` (or its shorthand `p`) command:

   ```bash
   (gdb) print variable_name
   ```

   For fields within a structure:
   ```bash
   (gdb) print structure_name.field_name
   ```

6. **Listing Source Code**

   If you have debug symbols included in your executable and have access to the source code, you can also list parts of the source code with the `list` command:

   ```bash
   (gdb) list
   ```

7. **Exiting GDB**

   To exit GDB, use:
   ```bash
   (gdb) quit
   ```

**Important Note:** For meaningful debugging information, make sure your executable is compiled with debug information. For GCC, this means using the `-g` flag during compilation:

```bash
gcc -g -o my_program my_program.c
```

Without the debug information, GDB will still be able to give you some information, but things like variable names and line numbers might be missing.