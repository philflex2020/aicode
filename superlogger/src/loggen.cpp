#include <fstream>
#include <iostream>
#include <string>
#include <filesystem>
#include <ctime>
#include <iomanip>

namespace fs = std::filesystem;

class Logger {
private:
    std::string project;
    fs::path baseDir;

public:
    Logger(const std::string& project)
        : project(project), baseDir("/var/log/" + project) {
        // Ensure baseDir exists
        fs::create_directories(baseDir);
    }

    void log(const std::string& userID, const std::string& logID, const std::string vars[4]) {
        // Ensure user directory exists
        fs::path userDir = baseDir / userID;
        fs::create_directories(userDir);

        // Open log file
        std::ofstream file(userDir / logID, std::ios_base::app);
        if (!file) {
            std::cerr << "Error opening log file\n";
            return;
        }

        // Get current time
        auto t = std::time(nullptr);
        auto tm = *std::localtime(&t);

        // Write log entry
        //file << std::put_time(&tm, "%FT%T") << "\t" << logID << "\t" << vars[0] << "\t" << vars[1] << "\t" << vars[2] << "\t" << vars[3] << "\n";
        file << std::put_time(&tm, "%FT%T") << "\t" << vars[0] << "\t" << vars[1] << "\t" << vars[2] << "\t" << vars[3] << "\n";
    }
};

int main() {
    Logger logger("testProject2");

    std::string vars[4] = {"var1", "var2", "var3", "var4"};
    logger.log("testUser1", "logID1", vars);
    logger.log("testUser2", "logID1", vars);
    logger.log("testUser3", "logID2", vars);
    logger.log("testUser1", "logID2", vars);
    logger.log("testUser2", "logID1", vars);
    logger.log("testUser3", "logID1", vars);
    logger.log("testUser1", "logID1", vars);
    logger.log("testUser2", "logID3", vars);
    logger.log("testUser3", "logID1", vars);

    return 0;
}
