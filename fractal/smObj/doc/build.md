To automate the process of building shared libraries from source files in a `libs` directory and compiling target executables by linking these shared objects, you can implement a `build` function in C++. This function will:

1. **Scan the `libs` Directory:** Identify all subdirectories, each representing a separate library.

2. **Compile Sources into Shared Objects:** For each library subdirectory, compile the source files into `.so` files, placing them in a `build` directory.

3. **Scan the `targ` Directory:** Identify all subdirectories, each representing a target executable.

4. **Compile and Link Targets:** For each target subdirectory, compile the source files and link them with all the `.so` files from the `build` directory to produce the final executables.

Here's how you can implement this in C++:

```cpp
#include <iostream>
#include <filesystem>
#include <vector>
#include <cstdlib>

namespace fs = std::filesystem;

void build() {
    const std::string libs_dir = "./libs";
    const std::string build_dir = "./build";
    const std::string targ_dir = "./targ";

    // Create build directory if it doesn't exist
    if (!fs::exists(build_dir)) {
        fs::create_directory(build_dir);
    }

    // Vector to store paths of compiled shared objects
    std::vector<std::string> shared_objects;

    // Scan libs directory for libraries
    for (const auto& lib_entry : fs::directory_iterator(libs_dir)) {
        if (lib_entry.is_directory()) {
            std::string lib_name = lib_entry.path().filename().string();
            std::string lib_build_dir = build_dir + "/" + lib_name;

            // Create library build directory if it doesn't exist
            if (!fs::exists(lib_build_dir)) {
                fs::create_directory(lib_build_dir);
            }

            // Compile each source file in the library directory into a shared object
            for (const auto& src_entry : fs::directory_iterator(lib_entry.path())) {
                if (src_entry.path().extension() == ".cpp") {
                    std::string src_file = src_entry.path().string();
                    std::string so_file = lib_build_dir + "/lib" + lib_name + ".so";
                    std::string compile_command = "g++ -shared -fPIC -o " + so_file + " " + src_file;
                    std::cout << "Compiling: " << compile_command << std::endl;
                    if (system(compile_command.c_str()) == 0) {
                        shared_objects.push_back(so_file);
                    } else {
                        std::cerr << "Error compiling " << src_file << std::endl;
                    }
                }
            }
        }
    }

    // Scan targ directory for targets
    for (const auto& targ_entry : fs::directory_iterator(targ_dir)) {
        if (targ_entry.is_directory()) {
            std::string targ_name = targ_entry.path().filename().string();
            std::string target_executable = build_dir + "/" + targ_name;

            // Collect all source files in the target directory
            std::vector<std::string> target_sources;
            for (const auto& src_entry : fs::directory_iterator(targ_entry.path())) {
                if (src_entry.path().extension() == ".cpp") {
                    target_sources.push_back(src_entry.path().string());
                }
            }

            // Construct the compile and link command
            std::string compile_command = "g++ -o " + target_executable;
            for (const auto& src : target_sources) {
                compile_command += " " + src;
            }
            for (const auto& so : shared_objects) {
                compile_command += " " + so;
            }
            std::cout << "Building target: " << compile_command << std::endl;
            if (system(compile_command.c_str()) != 0) {
                std::cerr << "Error building target " << targ_name << std::endl;
            }
        }
    }
}

int main() {
    build();
    return 0;
}
```

**Explanation:**

- **Directory Management:** The function uses the `<filesystem>` library to navigate through directories and manage paths.

- **Compilation Commands:** It constructs shell commands to compile source files into shared objects and to link these objects into executables.

- **Error Handling:** The function checks the return value of the `system` calls to detect compilation or linking errors.

**Assumptions:**

- Each subdirectory in `libs` contains source files for one library.

- Each subdirectory in `targ` contains source files for one target executable.

- The `build` directory is used to store both the compiled shared objects and the final executables.

**Note:** Ensure that the necessary permissions are set for creating directories and executing shell commands. Additionally, handle any dependencies between libraries appropriately, as this script assumes independent libraries.

This approach automates the build process, reducing manual compilation steps and ensuring consistency across builds. 