
#include <functional>
#include <vector>
#include <chrono>
#include <string>

enum class TransferMode {
    IMMEDIATE,
    PERIODIC_WITH_INPUT,
    PERIODIC_WITHOUT_INPUT
};

class DataTransferManager {
public:
    union Data {
        int intData;
        float floatData;
        bool boolData;
        std::string stringData;

        Data() {}
        ~Data() {}
    };

    using DataCallback = std::function<void(Data, std::chrono::system_clock::time_point)>;

    DataTransferManager(TransferMode mode, int periodMs = 1000);
    ~DataTransferManager();

    void setInputCallback(DataCallback callback);
    void addData(int data);
    void addData(float data);
    void addData(bool data);
    void addData(const std::string& data);
    void start();
    void stop();

private:
    TransferMode mode_;
    int periodMs_;
    DataCallback inputCallback_;
    std::vector<Data> lastDataInputs_;
    std::vector<std::chrono::system_clock::time_point> timestamps_;
    std::thread transferThread_;
    bool running_;

    void transferData();
};
