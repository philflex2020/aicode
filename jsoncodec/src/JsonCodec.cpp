#include <iostream>
#include <fstream>
#include <sstream>
#include <filesystem>
#include <map>
#include <nlohmann/json.hpp>
#include <zlib.h>

using json = nlohmann::json;
namespace fs = std::filesystem;

class JsonCodec {
private:
    std::map<std::string, int> keyDict;
    std::string dictFilePath;

    bool compressData(const std::string& data, std::string& compressedData) {
        // Compress the data using zlib compression
        const size_t bufferSize = 4096;
        char buffer[bufferSize];

        z_stream strm;
        strm.zalloc = Z_NULL;
        strm.zfree = Z_NULL;
        strm.opaque = Z_NULL;
        strm.avail_in = data.size();
        strm.next_in = reinterpret_cast<Bytef*>(const_cast<char*>(data.data()));

        if (deflateInit(&strm, Z_BEST_COMPRESSION) != Z_OK) {
            return false;
        }

        std::stringstream compressedStream;
        do {
            strm.avail_out = bufferSize;
            strm.next_out = reinterpret_cast<Bytef*>(buffer);
            if (deflate(&strm, Z_FINISH) == Z_STREAM_ERROR) {
                deflateEnd(&strm);
                return false;
            }
            compressedStream.write(buffer, bufferSize - strm.avail_out);
        } while (strm.avail_out == 0);

        deflateEnd(&strm);

        compressedData = compressedStream.str();
        return true;
    }

    bool decompressData(const std::string& compressedData, std::string& data) {
        // Decompress the data using zlib decompression
        const size_t bufferSize = 4096;
        char buffer[bufferSize];

        z_stream strm;
        strm.zalloc = Z_NULL;
        strm.zfree = Z_NULL;
        strm.opaque = Z_NULL;
        strm.avail_in = compressedData.size();
        strm.next_in = reinterpret_cast<Bytef*>(const_cast<char*>(compressedData.data()));

        if (inflateInit(&strm) != Z_OK) {
            return false;
        }

        std::stringstream dataStream;
        do {
            strm.avail_out = bufferSize;
            strm.next_out = reinterpret_cast<Bytef*>(buffer);
            if (inflate(&strm, Z_NO_FLUSH) == Z_STREAM_ERROR) {
                inflateEnd(&strm);
                return false;
            }
            dataStream.write(buffer, bufferSize - strm.avail_out);
        } while (strm.avail_out == 0);

        inflateEnd(&strm);

        data = dataStream.str();
        return true;
    }

public:
    JsonCodec(const std::string& dictFilePath) : dictFilePath(dictFilePath) {}

    bool loadKeyDictionary() {
        // Load the key dictionary from the specified file (if available).
        std::ifstream file(dictFilePath);
        if (!file) {
            return false;
        }

        keyDict.clear();
        std::string line;
        while (std::getline(file, line)) {
            std::istringstream iss(line);
            std::string key;
            int id;
            if (!(iss >> key >> id)) {
                continue;
            }
            keyDict[key] = id;
        }

        return true;
    }

    bool saveKeyDictionary() {
        // Save the key dictionary to the specified file.
        std::ofstream file(dictFilePath);
        if (!file) {
            return false;
        }

        for (const auto& entry : keyDict) {
            file << entry.first << " " << entry.second << "\n";
        }

        return true;
    }

    bool encodeAndCompress(const std::string& uri, const std::string& timestamp, const std::string& jsonBody) {
        // Encode the JSON data using the key dictionary and compress it.
        json jsonData = json::parse(jsonBody);

        // TODO: Implement encoding logic using keyDict and jsonData

        std::string encodedData = jsonBody; // Replace this with the actual encoding logic

        std::string compressedData;
        if (!compressData(encodedData, compressedData)) {
            return false;
        }

        // Create directories for URI and timestamp
        fs::create_directories(uri);
        fs::create_directories(uri + "/" + timestamp);

        // Save the compressed data to a binary file
        std::string outputFilePath = uri + "/" + timestamp + "/data.bin";
        std::ofstream outputFile(outputFilePath, std::ios::binary);
        if (!outputFile) {
            return false;
        }

        outputFile.write(compressedData.data(), compressedData.size());
        outputFile.close();

        return true;
    }

    bool decodeAndDecompress(const std::string& uri, const std::string& timestamp, std::string& outputJson) {
        // Decode the binary data using the key dictionary and decompress it.
        std::string inputFilePath = uri + "/" + timestamp + "/data.bin";
        std::ifstream inputFile(inputFilePath, std::ios::binary);
        if (!inputFile) {
            return false;
        }

        inputFile.seekg(0, std::ios::end);
        std::streampos fileSize = inputFile.tellg();
        inputFile.seekg(0, std::ios::beg);

        std::string compressedData(fileSize, '\0');
        if (!inputFile.read(compressedData.data(), fileSize)) {
            return false;
        }

        inputFile.close();

        std::string encodedData;
        if (!decompressData(compressedData, encodedData)) {
            return false;
        }

        // TODO: Implement decoding logic using keyDict and encodedData
        outputJson = encodedData; // Replace this with the actual decoding logic

        return true;
    }
};

int xxmain() {
    // Example usage:
    JsonCodec codec("/path/to/key_dict.json");

    std::string uri = "/var/jsoncodec/uri";
    std::string timestamp = "2023-08-10 12:34:56";
    std::string jsonBody = R"({"key1": "value1", "key2": "value2"})";

    if (codec.loadKeyDictionary()) {
        std::cout << "Key dictionary loaded successfully." << std::endl;
    } else {
        std::cerr << "Error loading key dictionary." << std::endl;
    }

    if (codec.encodeAndCompress(uri, timestamp, jsonBody)) {
        std::cout << "JSON data encoded and compressed successfully." << std::endl;
    } else {
        std::cerr << "Error encoding and compressing JSON data." << std::endl;
    }

    std::string decodedJson;
    if (codec.decodeAndDecompress(uri, timestamp, decodedJson)) {
        std::cout << "Decoded JSON data: " << decodedJson << std::endl;
    } else {
        std::cerr << "Error decoding and decompressing JSON data." << std::endl;
    }

    if (codec.saveKeyDictionary()) {
        std::cout << "Key dictionary saved successfully." << std::endl;
    } else {
        std::cerr << "Error saving key dictionary." << std::endl;
    }

    return 0;
}
