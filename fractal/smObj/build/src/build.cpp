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