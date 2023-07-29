# TimeTracker and TimeStore System Documentation

## Table of Contents
1. [Introduction](#introduction)
2. [Overview](#overview)
3. [TimeTracker Class](#timetracker-class)
   - 3.1 [Constructor](#constructor)
   - 3.2 [recordSample()](#recordsample)
   - 3.3 [getJsonOutput()](#getjsonoutput)
4. [TimeStore Class](#timestore-class)
   - 4.1 [Constructor](#constructor-1)
   - 4.2 [storeSample()](#storesample)
   - 4.3 [Thread Safety](#thread-safety)
5. [Directory Structure](#directory-structure)
6. [Build Instructions](#build-instructions)
7. [Usage](#usage)
8. [Testing](#testing)
9. [Future Improvements](#future-improvements)
10. [License](#license)

## 1. Introduction <a name="introduction"></a>
The TimeTracker and TimeStore system is designed to create a list of named objects and record time intervals (in nanoseconds) for each object. It then calculates and stores the maximum, minimum, and average time for each sample. The TimeStore runs on a separate thread to handle the storage of collected time samples.

## 2. Overview <a name="overview"></a>
The system consists of two main classes:
- **TimeTracker**: This class creates and manages a list of named objects and records time intervals for each object. It also provides functionality to calculate the maximum, minimum, and average time for each sample.
- **TimeStore**: This class runs on a separate thread and handles the storage of collected time samples. It stores the data in a simulated manner by printing the information to the console.

## 3. TimeTracker Class <a name="timetracker-class"></a>
The TimeTracker class provides the following functionalities:

### 3.1 Constructor <a name="constructor"></a>
- `TimeTracker()`: Creates a TimeTracker object.

### 3.2 recordSample() <a name="recordsample"></a>
- `void recordSample(const std::string& name, unsigned long long time)`: Records a time sample for a named object. If the object does not exist in the list, it will be added. If the object already exists, the time sample will be used to update the maximum, minimum, and average time for that object.

### 3.3 getJsonOutput() <a name="getjsonoutput"></a>
- `std::string getJsonOutput(const std::string& name) const`: Retrieves a JSON-formatted string containing the statistics (max, min, avg) for the named object. If the object does not exist, an empty JSON object will be returned.

## 4. TimeStore Class <a name="timestore-class"></a>
The TimeStore class provides the following functionalities:

### 4.1 Constructor <a name="constructor-1"></a>
- `TimeStore()`: Creates a TimeStore object. It also starts a separate thread to handle sample storage.

### 4.2 storeSample() <a name="storesample"></a>
- `void storeSample(const std::string& name, unsigned long long time)`: Stores a time sample in the TimeStore. In a real-world scenario, this data would be stored in a database or file. However, in this implementation, the data is printed to the console to simulate the storage process.

### 4.3 Thread Safety <a name="thread-safety"></a>
The TimeStore class is designed to be thread-safe. It uses a mutex and condition variable to ensure safe access to the shared data when multiple threads call the `storeSample()` function.

## 5. Directory Structure <a name="directory-structure"></a>
The system follows the following directory structure:
```
time_tracker/
include/
time_tracker.h
time_store.h
src/
time_tracker.cpp
time_store.cpp
test/
time_tracker_test.cpp
main.cpp
Makefile
css
```

## 6. Build Instructions <a name="build-instructions"></a>
To build the main program `timetracker` and the test program `test_timetracker`, use the provided Makefile. Run the following commands in the terminal:

```
$ cd time_tracker
$ make
```

The executable files `timetracker` and `test_timetracker` will be generated in the `time_tracker` directory.

## 7. Usage <a name="usage"></a>
The main program `timetracker` demonstrates how to use the TimeTracker class. The `main.cpp` file contains an example of how to record time samples and retrieve JSON-formatted statistics for the named objects.

## 8. Testing <a name="testing"></a>
To run the tests for the TimeTracker and TimeStore classes, use the test program `test_timetracker`:

```
$ cd time_tracker
$ make test
```

The tests will execute, and the test results will be displayed in the console.

## 9. Future Improvements <a name="future-improvements"></a>
- Enhance the TimeStore class to perform actual data storage in a database or file.
- Implement a more sophisticated JSON formatting for the statistics output.
- Extend the TimeTracker to support additional statistics, such as median and variance.

## 10. License <a name="license"></a>
The TimeTracker and TimeStore system is distributed under the [MIT License](https://opensource.org/lice