#ifndef JSON_CODEC_H
#define JSON_CODEC_H

#include <string>
#include <nlohmann/json.hpp>
#include <unordered_map>
#include <fstream>


using json = nlohmann::json;

class JsonCodec {
private:
    std::string uri;
    std::string timestamp;
    std::string jsonBody;
    std::string encodedFile;
    std::string timestampDir;
    std::string keyFile;
    std::string outputDir;

    std::unordered_map<std::string, int> KeyDict;
    json jKeyDict;
    std::unordered_map<int, std::string> ReverseKeyDict;
    // Helper function to populate the reverse lookup
    void populateReverseKeyDict();
;


public:
    JsonCodec(const std::string& uri, const std::string& timestamp, const std::string& jsonBody);
    std::string encode();
    std::unordered_map<std::string, int>* getKeyDict()
    {
        return &KeyDict;
    }
    int loadKeyFile(const std::string& keyFilePath);
    int saveKeyFile(const std::string& keyFilePath);
    json extractJsonData(const std::string& encodedData) const;
    json uncompressJsonData(std::istream& inputStream) const;
    json xxuncompressJsonData(std::istream& inputStream) const;
//    std::istream& readFileToStream(const std::string& filePath, std::istream& inputStream);

    // Function to get the index from a key
    int getIndex(std::string& key);

    // Function to get the key name from an index
    std::string getName(int index) const;

    // Function to read KeyDict from a JSON file
    void readKeyDictFromFile(const std::string& keyDictFilePath);
    int  savejKeyFile(const std::string& keyFilePath);


private:
    void compressJsonData(const json& jsonData, std::ostream& outputStream);
    void writeCompressedData(const std::string& filename, const std::string& data);
};

#endif // JSON_CODEC_H
