#include <iostream>
#include <sstream>
#include <chrono>
#include <any>
#include <vector>

#include <spdlog/spdlog.h>
#include "spdlog/details/fmt_helper.h" // for microseconds formattting
#include "spdlog/fmt/fmt.h"
#include "spdlog/fmt/bundled/ranges.h"
#include "spdlog/fmt/chrono.h"

//g++ -std=c++17 -DSPDLOG_COMPILED_LIB -o t2 fmt_test.cpp -lspdlog

template<typename T>
T getAnyVal(const std::any& anyVal, const T& defaultValue) {
    try {
        return std::any_cast<T>(anyVal);
    } catch (const std::bad_any_cast&) {
        return defaultValue;
    }
}

double  stringstream_any_example() {
    std::string prealloc;
    prealloc.reserve(80000);
    std::stringstream ss(prealloc);

    std::vector<std::any> myint;
    std::vector<std::any> myfloat;
    std::vector<std::any> mystring;

    for (int i = 0; i < 1000; ++i) {
        myint.push_back((int)i);
        myfloat.push_back ((float)(i*0.1));
        mystring.push_back (std::string("hi there"));
    }
    auto start = std::chrono::high_resolution_clock::now();
    // for (int i = 0; i < 1000; ++i) {
    //     ss << "This is a complex example with numbers: " << std::any_cast<int>(myint[i]) 
    //              << " floats: "                          << std::any_cast<float>(myfloat[i]) 
    //              << " and strings: "                     << std::any_cast<std::string>(mystring[i]) 
    //              << ". Repeating!";
    // }
    for (int i = 0; i < 1000; ++i) {
        ss << "This is a complex example with numbers: " << getAnyVal(myint[i], (int)0) 
                 << " floats: "                          << getAnyVal(myfloat[i], (float)0) 
                 << " and strings: "                     << getAnyVal(mystring[i], std::string("def")) 
                 << ". Repeating!";
    }

    auto end = std::chrono::high_resolution_clock::now();
    std::chrono::duration<double> diff = end-start;
    //std::cout << "StringStream Result: " << ss.str() << std::endl;
    std::cout << "StringStr any Time:  "                << diff.count()*1000.0 << " mS";
    std::cout << "  size: "                             << ss.str().size() << std::endl;
    return diff.count();
}

double  stringstream_example() {
    std::string prealloc;
    prealloc.reserve(80000);
    std::stringstream ss(prealloc);
    std::string mystring("hi there");

    auto start = std::chrono::high_resolution_clock::now();
    for (int i = 0; i < 1000; ++i) {
        ss << "This is a complex example with numbers: " << i << " floats: " 
                                                         << i * 0.1 
                                                         << " and strings: "<< mystring
                                                         <<  ". Repeating!";
    }
    auto end = std::chrono::high_resolution_clock::now();
    std::chrono::duration<double> diff = end-start;
    //std::cout << "StringStream Result: " << ss.str() << std::endl;
    std::cout << "StringStream Time:   " << diff.count()*1000.0 << " mS";
    std::cout << "  size: " << ss.str().size() << std::endl;
    return diff.count();
}

double spdlog_example() {
    spdlog::memory_buf_t buf;
    buf.reserve(80000);

    auto start = std::chrono::high_resolution_clock::now();
    for (int i = 0; i < 1000; ++i) {
        //buf.clear();
        fmt::format_to(std::back_inserter(buf), "This is a complex example with numbers: {}  floats: {} and strings {}. Repeating!", i, i * 0.1, "hi there" );
    }
    auto end = std::chrono::high_resolution_clock::now();
    std::chrono::duration<double> diff = end-start;
    std::cout << "Spdlog Time:         " << diff.count()*1000.0 << " mS";
    std::cout << "  size: " << buf.size() << std::endl;
    return diff.count();
}

int main() {
    double ssTotal = 0.0 ;
    double spTotal = 0.0 ;
    double saTotal = 0.0 ;
    for ( int i = 0 ; i <16 ; ++i)
    {
        saTotal += stringstream_any_example();
        ssTotal += stringstream_example();
        spTotal += spdlog_example();
    }
    std::cout << "Final SS     Time:         " << ssTotal*1000.0/16.0 << " mS\n";
    std::cout << "Final Spdlog Time:         " << spTotal*1000.0/16.0 << " mS\n";
    std::cout << "Final ss any Time:         " << saTotal*1000.0/16.0 << " mS\n";

    return 0;
}


void usingStringstream(int a, double b, const std::string &c) {
    std::stringstream ss;
    ss << "Integer: " << a << ", Double: " << b << ", String: " << c;
    std::string result = ss.str();
    std::cout << result << std::endl;
}

void usingSpdlogFmt(int a, double b, const std::string &c) {
    std::string result = fmt::format("Integer: {}, Double: {}, String: {}", a, b, c);
    std::cout << result << std::endl;
}

int xmain() {
    int myInt = 42;
    double myDouble = 3.141592653589793;
    std::string myString = "Hello, World!";

    auto start = std::chrono::high_resolution_clock::now();
    usingStringstream(myInt, myDouble, myString);
    auto stop = std::chrono::high_resolution_clock::now();
    auto duration = std::chrono::duration_cast<std::chrono::microseconds>(stop - start);
    std::cout << "Time taken by usingStringstream: " << duration.count() << " microseconds" << std::endl;

    start = std::chrono::high_resolution_clock::now();
    usingSpdlogFmt(myInt, myDouble, myString);
    stop = std::chrono::high_resolution_clock::now();
    duration = std::chrono::duration_cast<std::chrono::microseconds>(stop - start);
    std::cout << "Time taken by usingSpdlogFmt: " << duration.count() << " microseconds" << std::endl;

    return 0;
}


// `std::stringstream` in C++ doesn't inherently provide the same level of format control as `fmt` or `printf` style formatting. `fmt` is designed to offer Python-like format string syntax, making it more expressive and easier to use than `stringstream`.

// However, you can achieve better format control in `stringstream` by using manipulators and other IO functions provided by the C++ standard library. Here’s an example of how you can achieve some format control:

// ### Example of Format Control with `stringstream`:

// ```cpp
// #include <iostream>
// #include <iomanip>
// #include <sstream>

// int main() {
//     std::stringstream ss;

//     // Setting precision
//     ss << std::fixed << std::setprecision(2);
//     ss << "This is a float: " << 3.14159 << std::endl;

//     // Setting width and fill character
//     ss << "This is an integer with width 5 and fill character '0': " 
//        << std::setw(5) << std::setfill('0') << 42 << std::endl;

//     // Formatting as hexadecimal
//     ss << "This is a hexadecimal: " << std::hex << 255 << std::endl;

//     // Resetting to decimal format
//     ss << std::dec;

//     // Using boolalpha to print booleans as true/false
//     ss << "This is a boolean: " << std::boolalpha << true << std::endl;

//     std::cout << ss.str();

//     return 0;
// }
// ```

// ### Explanation:

// - `std::fixed` and `std::setprecision(n)` are used to format floating-point numbers with `n` digits after the decimal point.
// - `std::setw(n)` sets the width of the next input/output field.
// - `std::setfill(c)` sets the fill character for padding.
// - `std::hex` and `std::dec` are used to switch between hexadecimal and decimal format.
// - `std::boolalpha` is used to print booleans as `true` or `false`.

